// SPDX-License-Identifier: GPL-3.0-only
#include "SetupBridge.h"
#include "SetupCommandLine.h"
#include "SetupNative.h"
#include "SetupResources.h"
#include "Localization.h"
#include "AppVersion.h"
#include "SetupUiServices.h"
#include "ProxyDetection.h"
#include "StartupApps.h"
#include <shellapi.h>
#include <objbase.h>
#include <cstring>
#include <memory>
#include <new>

namespace {
using automation::Json;
constexpr size_t MaxResponseBytes = 1024 * 1024;

setup::Result ResolvePaths(const uint8_t* bytes, size_t size, setup::Paths& paths) {
    if (!bytes && !size) return setup::GetPaths(paths);
    if (!bytes || !size || size > automation::contract::JsonBytes) throw automation::Invalid("invalid-paths");
    const auto value = automation::ParseJson(std::string(reinterpret_cast<const char*>(bytes), size));
    value.Fields({L"defaultDirectory", L"startupLink", L"settingsLink", L"desktopLink", L"codexEnvFile",
        L"legacyStartupLink", L"legacyDesktopLink"});
    auto get = [&](PCWSTR key) {
        const auto& path = value.At(key).String();
        if (path.empty()) throw automation::Invalid("missing-isolated-path");
        return automation::FullPath(path);
    };
    paths = {get(L"defaultDirectory"), get(L"startupLink"), get(L"settingsLink"),
        get(L"desktopLink"), get(L"codexEnvFile"), get(L"legacyStartupLink"), get(L"legacyDesktopLink")};
    return {};
}

bool isolatedDark = true, isolatedCodexEnv = true;
bool startupReminderShown = false;

void Deliver(const Json& value, SetupBridgeBuffer& output) {
    const auto bytes = automation::WriteJson(value);
    if (bytes.size() > MaxResponseBytes) throw automation::Invalid("bridge-response-too-large");
    auto memory = std::make_unique<uint8_t[]>(bytes.size());
    std::memcpy(memory.get(), bytes.data(), bytes.size());
    output.length = bytes.size();
    output.data = memory.release();
}

Json Options(const setup::Options& o) {
    auto j = Json::Object();
    j[L"startup"] = Json(o.startup); j[L"silent"] = Json(o.silent);
    j[L"proxy"] = Json(o.proxy); j[L"desktop"] = Json(o.desktop); j[L"codexEnv"] = Json(o.codexEnv);
    j[L"protocol"] = Json(o.protocol); j[L"host"] = Json(o.host); j[L"port"] = Json(o.port);
    j[L"bypass"] = Json(o.bypass); j[L"extra"] = Json(o.extra); j[L"directory"] = Json(o.directory);
    return j;
}

Json Preferences(bool isolated) {
    auto result = Json::Object();
    result[L"dark"] = Json(isolated ? isolatedDark : setup::ui::ReadDarkPreference());
    result[L"language"] = Json(l10n::FollowsSystem() ? L"auto" : l10n::Current().tag);
    result[L"resolvedLanguage"] = Json(l10n::Current().tag);
    return result;
}
Json Program(const std::wstring& path) {
    auto result = Json::Object();
    result[L"path"] = Json(path);
    result[L"name"] = Json(path.substr(path.find_last_of(L'\\') + 1));
    setup::Link link;
    if (companions::FileKind(path.c_str()) == companions::Kind::Link) setup::ReadLink(path, link);
    result[L"arguments"] = Json(link.arguments);
    return result;
}
Json Programs(const setup::Paths& paths) {
    auto result = Json::Array();
    const auto folder = setup::Join(paths.defaultDirectory, companions::Folder);
    WIN32_FIND_DATAW data{};
    const auto search = FindFirstFileW(setup::Join(folder, L"*").c_str(), &data);
    if (search == INVALID_HANDLE_VALUE) return result;
    struct Search { HANDLE value; ~Search() { FindClose(value); } } guard{search};
    do {
        if (!(data.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))
            && companions::FileKind(data.cFileName) != companions::Kind::Unknown) {
            if (result.array.size() >= 256) break;
            result.array.push_back(Program(setup::Join(folder, data.cFileName)));
        }
    } while (FindNextFileW(search, &data));
    return result;
}
setup::Options ParseOptions(const Json& value) {
    value.Fields({L"startup", L"silent", L"desktop", L"proxy", L"codexEnv", L"protocol", L"host", L"port", L"bypass", L"extra", L"directory"});
    setup::Options result;
    result.startup = value.At(L"startup").Bool(); result.silent = value.At(L"silent").Bool();
    result.desktop = value.At(L"desktop").Bool(); result.proxy = value.At(L"proxy").Bool(); result.codexEnv = value.At(L"codexEnv").Bool();
    result.protocol = value.At(L"protocol").String(); result.host = value.At(L"host").String(); result.port = value.At(L"port").String();
    result.bypass = value.At(L"bypass").String(); result.extra = value.At(L"extra").String(); result.directory = value.At(L"directory").String();
    return result;
}
uint32_t CommitCode(setup::CommitState commit) noexcept {
    switch (commit) {
    case setup::CommitState::Committed: return 1;
    case setup::CommitState::RolledBack: return 2;
    case setup::CommitState::Incomplete: return 3;
    default: return 0;
    }
}
void Require(const setup::Result& result) {
    if (!result) throw result;
}
// Only harness paths reach here. All writes and chosen files must stay under its root.
void CheckIsolatedPath(const setup::Paths& paths, const std::wstring& path, bool isolated) {
    if (!isolated) return;
    const auto root = paths.defaultDirectory.substr(0, paths.defaultDirectory.find_last_of(L'\\'));
    const auto full = automation::FullPath(path);
    if (full.size() <= root.size() || full[root.size()] != L'\\'
        || CompareStringOrdinal(full.c_str(), static_cast<int>(root.size()), root.c_str(), static_cast<int>(root.size()), TRUE) != CSTR_EQUAL)
        throw automation::Invalid("outside-test-root");
    // A junction in a selected test directory must not escape the isolated tree.
    for (size_t end = root.size(); end <= full.size();) {
        const auto part = full.substr(0, end);
        const DWORD attrs = GetFileAttributesW(part.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_REPARSE_POINT)) throw automation::Invalid("redirected-test-path");
        const auto next = full.find(L'\\', end + 1);
        if (end == full.size()) break;
        end = next == full.npos ? full.size() : next;
    }
}

// Isolated CLI tests never inspect or change real Windows startup settings.
setup::StartupReport IsolatedStartup() { return {}; }
}

extern "C" void setup_bridge_initialize() noexcept { l10n::Initialize(); }

extern "C" void setup_bridge_free(SetupBridgeBuffer* buffer) noexcept {
    if (!buffer) return;
    delete[] buffer->data;
    *buffer = {};
}

extern "C" uint32_t setup_bridge_read_state(const uint8_t* pathBytes, size_t size,
    SetupBridgeBuffer* output) noexcept {
    if (!output || output->data || output->length) return ERROR_INVALID_PARAMETER;
    try {
        setup::Paths paths;
        const auto pathsResult = ResolvePaths(pathBytes, size, paths);
        if (!pathsResult) return pathsResult.code;
        const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (FAILED(com)) return static_cast<uint32_t>(com);
        struct Apartment { ~Apartment() { CoUninitialize(); } } apartment;
        automation::UserLock operation;
        const DWORD locked = operation.Acquire(paths.settingsLink);
        if (locked) return locked;
        setup::Loaded loaded;
        const auto status = setup::Load(paths, loaded);
        if (!status) {
            auto error = Json::Object();
            error[L"nativeError"] = Json::Number(status.code);
            error[L"message"] = Json(status.message);
            error[L"commit"] = Json(L"not-started");
            Deliver(error, *output);
            return status.code;
        }
        // Test paths imply deterministic preferences; production never supplies them.
        const bool isolated = pathBytes != nullptr;
        loaded.options.codexEnv = isolated ? isolatedCodexEnv : setup::ui::ReadCodexEnvPreference();
        auto state = Json::Object();
        state[L"schemaVersion"] = Json::Number(1);
        state[L"setupVersion"] = Json(L"" APP_VERSION_TEXT);
        state[L"deployedVersion"] = Json(loaded.deployedVersion);
        state[L"deployed"] = Json(loaded.deployed);
        state[L"hasSettings"] = Json(loaded.hasSettings);
        state[L"validArguments"] = Json(loaded.validArguments);
        state[L"notice"] = Json(loaded.notice);
        state[L"options"] = Options(loaded.options);
        state[L"defaultDirectory"] = Json(paths.defaultDirectory);
        state[L"programs"] = Programs(paths);
        state[L"preferences"] = Preferences(isolated);
        const HMODULE module = GetModuleHandleW(nullptr);
        const HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(IDR_LAUNCHER), RT_RCDATA);
        state[L"payloadBytes"] = Json::Number(resource ? SizeofResource(module, resource) : 0);
        Deliver(state, *output);
        return ERROR_SUCCESS;
    } catch (const std::bad_alloc&) { return ERROR_NOT_ENOUGH_MEMORY; }
    catch (const automation::IoError& error) { return error.code; }
    catch (...) { return ERROR_INVALID_DATA; }
}

extern "C" uint32_t setup_bridge_action(uint32_t action, const uint8_t* requestBytes, size_t requestLength,
    const uint8_t* pathBytes, size_t pathsLength, uintptr_t owner,
    SetupBridgeBuffer* output, uint32_t* commit) noexcept {
    if (!output || output->data || output->length || !commit) return ERROR_INVALID_PARAMETER;
    *commit = 0;
    try {
        if (!requestBytes || !requestLength || requestLength > automation::contract::JsonBytes) return ERROR_INVALID_PARAMETER;
        const auto request = automation::ParseJson(std::string(reinterpret_cast<const char*>(requestBytes), requestLength));
        setup::Paths paths;
        Require(ResolvePaths(pathBytes, pathsLength, paths));
        const bool isolated = pathBytes != nullptr;
        const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (FAILED(com)) return static_cast<uint32_t>(com);
        struct Apartment { ~Apartment() { CoUninitialize(); } } apartment;
        auto response = Json::Object();
        if (action == SETUP_VALIDATE) {
            auto options = ParseOptions(request);
            std::wstring arguments, directory;
            auto result = setup::BuildArguments(options, arguments);
            if (result) result = setup::NormalizeDirectory(options.directory, directory);
            response[L"valid"] = Json(static_cast<bool>(result));
            response[L"arguments"] = Json(arguments);
            response[L"errors"] = Json::Object();
            response[L"detail"] = Json(result.message);
            response[L"nativeError"] = Json::Number(result.code);
        } else if (action == SETUP_DETECT) {
            request.Fields({L"protocol", L"host"});
            const auto& protocol = request.At(L"protocol").String(16);
            const auto& host = request.At(L"host").String(1024);
            // Isolated sessions never inspect the user's proxy applications/configuration.
            setup::ProxyDetection found;
            if (!isolated) found = setup::DetectProxyPort(protocol, host);
            else if (host != L"127.0.0.1" && host != L"localhost") found.message = l10n::Text("proxy.localonly");
            else if (protocol != L"http" && protocol != L"socks5") found.message = l10n::Text("proxy.httpsmanual");
            else found = {L"7897", l10n::Format("proxy.detected", {{"source", l10n::Text("ui.detect.fixture")}}), l10n::Text("ui.detect.fixture")};
            response[L"port"] = Json(found.port); response[L"message"] = Json(found.message); response[L"source"] = Json(found.source);
        } else if (action == SETUP_SAVE) {
            auto options = ParseOptions(request);
            CheckIsolatedPath(paths, options.directory, isolated);
            automation::UserLock operation;
            const DWORD locked = operation.Acquire(paths.settingsLink);
            if (locked) throw setup::Result{locked, l10n::Text("error.busy")};
            setup::Loaded previous;
            Require(setup::Load(paths, previous));
            const HMODULE module = GetModuleHandleW(nullptr);
            const HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(IDR_LAUNCHER), RT_RCDATA);
            const auto loadedResource = resource ? LoadResource(module, resource) : nullptr;
            const auto payload = loadedResource ? LockResource(loadedResource) : nullptr;
            if (!payload) throw automation::IoError(ERROR_RESOURCE_DATA_NOT_FOUND);
            // Normalize before writes; the UI receives the exact deployed value.
            std::wstring normalized;
            Require(setup::NormalizeDirectory(options.directory, normalized));
            options.directory.swap(normalized);
            std::wstring target;
            const auto result = setup::Apply(paths, options, {static_cast<const BYTE*>(payload), SizeofResource(module, resource)}, target);
            *commit = CommitCode(result.commit); // No allocation between Apply and publishing its outcome.
            const bool saved = *commit == 1;
            auto tasks = Json::Array();
            auto warnings = Json::Array();
            std::wstring detail = result.message;
            PCWSTR message = *commit == 1 ? L"setup.status.nextlaunch" : *commit == 2 ? L"ui.save.rollback"
                : *commit == 3 ? L"ui.save.incomplete" : L"ui.save.failed";
            if (saved) {
                const bool wroteEnv = options.proxy && options.codexEnv;
                const bool startup = options.startup && !startupReminderShown;
                setup::StartupReport report;
                auto closed = setup::ChatGPTCloseResult::NeedsManualExit;
                if (!isolated) setup::ui::CompleteSave(startup, wroteEnv, report, closed);
                // In test sessions the actions remain explicit follow-ups, never real OS changes.
                if (isolated) isolatedCodexEnv = options.codexEnv;
                else if (!setup::ui::SaveCodexEnvPreference(options.codexEnv)) warnings.array.emplace_back(L"setup.status.envunsaved");
                if (wroteEnv) {
                    message = closed == setup::ChatGPTCloseResult::Closed ? L"setup.quit.closed"
                        : closed == setup::ChatGPTCloseResult::NotRunning ? L"setup.quit.absent" : L"setup.quit.manual";
                    if (closed == setup::ChatGPTCloseResult::NeedsManualExit) tasks.array.emplace_back(L"restart");
                } else if (!previous.managedTarget.empty() && !automation::SamePath(previous.managedTarget, target)) message = L"setup.status.keptold";
                if (startup) {
                    startupReminderShown = true;
                    tasks.array.emplace_back(L"startup");
                    if (!isolated) detail = report.Message();
                }
            }
            response[L"commit"] = Json(*commit == 1 ? L"saved" : *commit == 2 ? L"rolled-back" : *commit == 3 ? L"rollback-incomplete" : L"not-started");
            response[L"message"] = Json(message); response[L"detail"] = Json(detail);
            response[L"tasks"] = std::move(tasks); response[L"warnings"] = std::move(warnings);
            response[L"conflictDirectory"] = Json(result.conflictDirectory);
            response[L"nativeError"] = Json::Number(result.code);
            response[L"options"] = Options(options);
        } else if (action == SETUP_DIRECTORY || action == SETUP_ADD_PROGRAM) {
            const bool program = action == SETUP_ADD_PROGRAM;
            request.Fields({L"initial", L"testSelection"});
            std::wstring selected;
            const auto initial = request.Has(L"initial") ? request.At(L"initial").String() : paths.defaultDirectory;
            if (request.Has(L"testSelection")) {
                if (!isolated) throw automation::Invalid("test-selection-disabled");
                selected = request.At(L"testSelection").String();
            } else Require(setup::ui::ChoosePath(reinterpret_cast<HWND>(owner), program, initial, selected));
            if (!selected.empty()) CheckIsolatedPath(paths, selected, isolated);
            if (program && !selected.empty()) {
                // Complete the reply allocation before the immediately effective write.
                response[L"entry"] = Program(selected);
                response[L"path"] = Json(selected);
                Deliver(response, *output);
                automation::UserLock operation;
                const DWORD locked = operation.Acquire(paths.settingsLink);
                if (locked) { setup_bridge_free(output); throw setup::Result{locked, l10n::Text("error.busy")}; }
                try { Require(setup::AddCompanionProgram(paths, selected)); }
                catch (...) { setup_bridge_free(output); throw; }
                *commit = 1;
                return ERROR_SUCCESS;
            } else response[L"entry"] = Json{};
            response[L"path"] = Json(selected);
        } else if (action == SETUP_OPEN_LOCATION) {
            request.Fields({L"location"});
            const auto& location = request.At(L"location").String(32);
            if (location != L"startup" && location != L"deployment" && location != L"windows") throw automation::Invalid("unknown-location");
            if (!isolated) {
                if (location == L"windows") Require(setup::OpenStartupSettings(reinterpret_cast<HWND>(owner)));
                else {
                    automation::UserLock operation;
                    const DWORD locked = operation.Acquire(paths.settingsLink);
                    if (locked) throw setup::Result{locked, l10n::Text("error.busy")};
                    std::wstring folder;
                    if (location == L"startup") Require(setup::EnsureCompanionFolder(paths, folder));
                    else { setup::Loaded loaded; Require(setup::Load(paths, loaded)); folder = loaded.options.directory; }
                    SHELLEXECUTEINFOW info{sizeof(info)};
                    info.fMask = SEE_MASK_FLAG_NO_UI; info.hwnd = reinterpret_cast<HWND>(owner);
                    info.lpVerb = L"open"; info.lpFile = folder.c_str(); info.nShow = SW_SHOWNORMAL;
                    if (!ShellExecuteExW(&info)) throw automation::IoError(GetLastError());
                }
            }
        } else if (action == SETUP_PREFERENCES) {
            request.Fields({L"dark", L"language"});
            // Validate the whole request before changing either preference.
            const bool dark = request.At(L"dark").Bool();
            const auto& language = request.At(L"language").String(32);
            if (language != L"auto" && language != L"zh-CN" && language != L"en-US" && language != L"ja-JP") throw automation::Invalid("unknown-language");
            if (language == L"auto") l10n::UseSystem(); else l10n::Select(language);
            auto warnings = Json::Array();
            if (isolated) isolatedDark = dark;
            else {
                if (!setup::ui::SaveDarkPreference(dark)) warnings.array.emplace_back(L"setup.status.themeunsaved");
                if (!setup::ui::SaveLanguagePreference()) warnings.array.emplace_back(L"setup.language.unsaved");
            }
            response[L"preferences"] = Preferences(isolated);
            response[L"warnings"] = std::move(warnings);
        } else throw automation::Invalid("unknown-ui-command");
        Deliver(response, *output);
        return ERROR_SUCCESS;
    } catch (const setup::Result& result) {
        try {
            auto error = Json::Object();
            error[L"nativeError"] = Json::Number(result.code); error[L"message"] = Json(result.message);
            error[L"commit"] = Json(L"not-started");
            Deliver(error, *output);
        } catch (...) {}
        return result.code;
    } catch (const std::bad_alloc&) { return ERROR_NOT_ENOUGH_MEMORY; }
    catch (const automation::IoError& error) { return error.code; }
    catch (...) { return ERROR_INVALID_DATA; }
}

extern "C" int32_t setup_bridge_headless(const uint16_t* tail, size_t tailLength,
    const uint8_t* pathBytes, size_t pathsLength) noexcept {
    static_assert(sizeof(wchar_t) == sizeof(uint16_t), "Windows UTF-16 ABI required");
    if (!tail || !tailLength || tailLength > 32766) return 20;
    try {
        const std::wstring arguments(reinterpret_cast<const wchar_t*>(tail), tailLength);
        if (arguments.find(L'\0') != arguments.npos) return 20;
        setup::Paths paths;
        if (pathBytes || pathsLength) {
            if (!ResolvePaths(pathBytes, pathsLength, paths)) return 50;
            return setup::RunSetupCommandLine(GetModuleHandleW(nullptr), arguments, &paths, IsolatedStartup);
        }
        return setup::RunSetupCommandLine(GetModuleHandleW(nullptr), arguments);
    } catch (...) { return 50; }
}

extern "C" int32_t setup_bridge_compatibility(const uint8_t* pathBytes, size_t size) noexcept {
    try {
        setup::Paths paths;
        if (pathBytes || size) {
            const auto status = ResolvePaths(pathBytes, size, paths);
            if (!status) return static_cast<int32_t>(status.code);
        }
        return setup::RunNativeSetup(GetModuleHandleW(nullptr), SW_SHOWNORMAL, true,
            pathBytes ? &paths : nullptr);
    } catch (const std::bad_alloc&) { return ERROR_NOT_ENOUGH_MEMORY; }
    catch (...) { return ERROR_INVALID_DATA; }
}

extern "C" void setup_bridge_show_bootstrap_error(uint32_t compatibilityFailed) noexcept {
    MessageBoxW(nullptr, l10n::Text(compatibilityFailed ? "setup.error.compatibility" : "setup.error.webviewstart"),
        l10n::Text("setup.error.open"), MB_OK | MB_ICONERROR);
}
