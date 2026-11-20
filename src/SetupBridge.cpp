// SPDX-License-Identifier: GPL-3.0-only
#include "SetupBridge.h"
#include "SetupCommandLine.h"
#include "SetupNative.h"
#include "SetupResources.h"
#include "Localization.h"
#include "AppVersion.h"
#include "SetupUiServices.h"
#include "SetupProgramIcons.h"
#include "ProxyDetection.h"
#include "StartupApps.h"
#include "CompanionManifest.h"
#include <shellapi.h>
#include <objbase.h>
#include <algorithm>
#include <cstring>
#include <memory>
#include <new>
#include <set>

namespace {
using automation::Json;
constexpr size_t MaxResponseBytes = 1024 * 1024;

setup::Result ResolvePaths(const uint8_t* bytes, size_t size, setup::Paths& paths) {
    if (!bytes && !size) return setup::GetPaths(paths);
    if (!bytes || !size || size > automation::contract::JsonBytes) throw automation::Invalid("invalid-paths");
    const auto value = automation::ParseJson({reinterpret_cast<const char*>(bytes), size});
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

void DeliverError(const setup::Result& result, SetupBridgeBuffer& output) {
    auto error = Json::Object();
    error[L"nativeError"] = Json::Number(result.code);
    error[L"message"] = Json(result.message);
    if (!result.conflictDirectory.empty()) error[L"conflictDirectory"] = Json(result.conflictDirectory);
    error[L"commit"] = Json(L"not-started");
    Deliver(error, output);
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
struct ProgramList {
    Json entries = Json::Array();
    std::vector<setup::ui::ProgramIconSource> icons;
    size_t bytes = 64; // Reserve the JSON envelope and separators within the IPC limit.
    bool incomplete = false;
    void Add(const std::wstring& id, const std::wstring& name, const std::wstring& path,
        const std::wstring& arguments, PCWSTR status, setup::ui::ProgramIconSource icon = {}) {
        auto row = Json::Object();
        row[L"id"] = Json(id); row[L"name"] = Json(name); row[L"path"] = Json(path);
        row[L"arguments"] = Json(arguments); row[L"status"] = Json(status);
        const auto size = automation::WriteJson(row).size() + 1;
        if (size > MaxResponseBytes - bytes) { incomplete = true; return; }
        bytes += size;
        entries.array.push_back(std::move(row));
        icons.push_back(std::move(icon));
    }
};
void ListRegistered(const std::wstring& root, ProgramList& list, std::vector<std::wstring>& activeTargets) {
    bool dispatchable = true;
    try { chain::ReadPolicy(root); }
    catch (const automation::Invalid&) { dispatchable = false; }
    catch (const automation::IoError&) { dispatchable = false; }
    std::vector<chain::Record> records;
    std::vector<size_t> order;
    const auto localDrive = [](const std::wstring& path) {
        return setup::ui::ClassifyProgramDrive(path) == setup::ui::ProgramDrive::Local;
    };
    if (!chain::ReadRegistryOrder(root, records, order, chain::RegistrationJson::Keep, localDrive)) dispatchable = false;
    if (!dispatchable) list.incomplete = true;
    std::set<std::wstring_view> blocked;
    if (dispatchable) for (const auto index : order) {
        const auto& record = records[index];
        if (!record.error.empty() || !record.entry.enabled) continue;
        for (const auto& dependency : record.entry.dependencies) {
            const auto dep = std::find_if(records.begin(), records.end(),
                [&](const auto& r) { return r.id == dependency.id; });
            // The list deliberately skips unverified drives. Readiness there is unknown.
            if (dep != records.end() && dep->error == L"manifest-drive-unavailable") continue;
            if (dep == records.end() || !dep->error.empty() || !dep->entry.enabled || blocked.count(dep->id)
                || (!dependency.minimumVersion.empty() && chain::CompareVersion(dep->entry.version, dependency.minimumVersion) < 0)) {
                blocked.insert(record.id);
                break;
            }
        }
    }
    for (auto& record : records) {
        if (record.error == L"manifest-drive-unavailable") {
            list.incomplete = true;
            list.Add(record.id, record.id, record.json.At(L"manifestPath").String(), L"", L"unavailable");
            continue;
        }
        if (!record.error.empty()) {
            list.Add(record.path, record.id.empty() ? L".json" : record.id, record.path, L"", L"invalid");
            continue;
        }
        auto& entry = record.entry;
        if (!entry.enabled) {
            // The dispatcher's disabled record deliberately does not open its package.
            // Only a matching, still valid manifest earns the protocol badge in the UI.
            try {
                entry = chain::ReadManifest(entry.manifestPath, record.json.At(L"overrides"), localDrive);
            }
            catch (const automation::Invalid&) {}
            catch (const automation::IoError&) {}
            if (entry.id != record.id || entry.name.empty()) {
                list.Add(record.id, record.id, record.entry.manifestPath, L"", L"disabled");
                continue;
            }
        }
        if (entry.enabled && dispatchable) activeTargets.push_back(entry.target);
        std::wstring arguments;
        for (const auto& argument : entry.arguments) {
            if (!arguments.empty()) arguments += L' ';
            arguments += automation::Quote(argument);
        }
        const auto status = !entry.enabled ? L"adapted-disabled" : !dispatchable ? L"invalid"
            : blocked.count(record.id) ? L"dependency-blocked" : L"adapted";
        list.Add(record.id, entry.name, entry.target, arguments, status);
    }
    // An unread package can change the dependency graph. Keep ordinary entries visible in that case.
    if (list.incomplete) activeTargets.clear();
}
void ListFolder(const std::wstring& folder, const std::vector<std::wstring>& activeTargets, ProgramList& list) {
    automation::CheckPlainPath(folder);
    WIN32_FIND_DATAW data{};
    const auto search = FindFirstFileExW(setup::Join(folder, L"*").c_str(), FindExInfoBasic, &data, FindExSearchNameMatch, nullptr, 0);
    if (search == INVALID_HANDLE_VALUE) {
        const auto error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) list.incomplete = true;
        return;
    }
    struct Search { HANDLE value; ~Search() { FindClose(value); } } guard{search};
    size_t count = 0;
    do {
        const auto kind = companions::FileKind(data.cFileName);
        if (kind == companions::Kind::Unknown || (data.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) continue;
        if (++count > 256) { list.incomplete = true; return; }
        const auto path = setup::Join(folder, data.cFileName);
        setup::Link link;
        setup::LinkIcon icon;
        const bool shortcut = kind == companions::Kind::Link;
        if (shortcut) setup::ReadLink(path, link, true, &icon);
        const auto& target = shortcut ? link.target : path;
        const bool valid = companions::ValidTarget(target.c_str(), link.arguments.c_str(), link.directory.c_str());
        if (!valid && !shortcut) continue;
        if (valid && std::any_of(activeTargets.begin(), activeTargets.end(), [&](const auto& active) {
            return automation::SamePath(active, target);
        })) continue;
        bool available = valid;
        if (valid && shortcut) {
            const auto drive = setup::ui::ClassifyProgramDrive(target);
            if (drive == setup::ui::ProgramDrive::Missing) available = false;
            else if (drive == setup::ui::ProgramDrive::Local) {
                const auto attrs = GetFileAttributesW(target.c_str());
                available = attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
            }
        }
        list.Add(path, data.cFileName, path, link.arguments, available ? L"ordinary" : L"unavailable",
            {available && shortcut ? link.target : L"", std::move(icon)});
    } while (FindNextFileW(search, &data));
    if (GetLastError() != ERROR_NO_MORE_FILES) list.incomplete = true;
}
Json Programs(const setup::Paths& paths, bool isolated) {
    ProgramList list;
    std::vector<std::wstring> activeTargets;
    try { ListRegistered(paths.defaultDirectory, list, activeTargets); }
    catch (const automation::Invalid&) { list.incomplete = true; }
    catch (const automation::IoError&) { list.incomplete = true; }
    try { ListFolder(setup::Join(paths.defaultDirectory, companions::Folder), activeTargets, list); }
    catch (const automation::Invalid&) { list.incomplete = true; }
    catch (const automation::IoError&) { list.incomplete = true; }
    auto response = Json::Object();
    response[L"icons"] = setup::ui::ProgramIcons(list.entries, list.icons, MaxResponseBytes - list.bytes,
        isolated ? automation::ParentPath(paths.defaultDirectory) : L"");
    std::sort(list.entries.array.begin(), list.entries.array.end(), [](const Json& a, const Json& b) {
        const auto order = CompareStringOrdinal(a.At(L"name").text.c_str(), -1, b.At(L"name").text.c_str(), -1, TRUE);
        return order == CSTR_EQUAL ? a.At(L"id").text < b.At(L"id").text : order == CSTR_LESS_THAN;
    });
    response[L"entries"] = std::move(list.entries);
    response[L"incomplete"] = Json(list.incomplete);
    return response;
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
struct CommitInfo { uint32_t code; PCWSTR name; PCWSTR message; };
constexpr CommitInfo DescribeCommit(setup::CommitState commit) noexcept {
    switch (commit) {
    case setup::CommitState::NotStarted: return {0, L"not-started", L"ui.save.failed"};
    case setup::CommitState::Committed: return {1, L"saved", L"setup.status.nextlaunch"};
    case setup::CommitState::RolledBack: return {2, L"rolled-back", L"ui.save.rollback"};
    case setup::CommitState::Incomplete: return {3, L"rollback-incomplete", L"ui.save.incomplete"};
    }
    return {3, L"rollback-incomplete", L"ui.save.unknown"};
}
void Require(const setup::Result& result) {
    if (!result) throw result;
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
        const bool isolated = pathBytes != nullptr;
        automation::UserLock operation;
        const DWORD locked = operation.AcquireForSetup(paths, isolated);
        if (locked) return locked;
        setup::Loaded loaded;
        const auto status = setup::Load(paths, loaded);
        if (!status) {
            DeliverError(status, *output);
            return status.code;
        }
        // Test paths imply deterministic preferences; production never supplies them.
        loaded.options.codexEnv = isolated ? isolatedCodexEnv : setup::ui::ReadCodexEnvPreference();
        auto state = Json::Object();
        state[L"schemaVersion"] = Json::Number(1);
        state[L"setupVersion"] = Json(L"" APP_RELEASE_VERSION);
        state[L"deployedVersion"] = Json(loaded.deployedVersion);
        state[L"deployed"] = Json(loaded.deployed);
        state[L"hasSettings"] = Json(loaded.hasSettings);
        state[L"validArguments"] = Json(loaded.validArguments);
        state[L"notice"] = Json(loaded.notice);
        state[L"options"] = Options(loaded.options);
        state[L"defaultDirectory"] = Json(paths.defaultDirectory);
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
        const auto request = automation::ParseJson({reinterpret_cast<const char*>(requestBytes), requestLength});
        setup::Paths paths;
        // A preview only checks the supplied text. Still validate any explicit harness paths.
        if (action != SETUP_VALIDATE || pathBytes || pathsLength) Require(ResolvePaths(pathBytes, pathsLength, paths));
        if (action == SETUP_VALIDATE) {
            const auto options = ParseOptions(request);
            std::wstring arguments, directory;
            auto result = setup::BuildArguments(options, arguments);
            if (result) result = setup::NormalizeDirectory(options.directory, directory);
            auto response = Json::Object();
            response[L"valid"] = Json(static_cast<bool>(result));
            response[L"arguments"] = Json(std::move(arguments));
            response[L"errors"] = Json::Object();
            response[L"detail"] = Json(std::move(result.message));
            response[L"nativeError"] = Json::Number(result.code);
            Deliver(response, *output);
            return ERROR_SUCCESS;
        }
        const bool isolated = pathBytes != nullptr;
        const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (FAILED(com)) return static_cast<uint32_t>(com);
        struct Apartment { ~Apartment() { CoUninitialize(); } } apartment;
        auto response = Json::Object();
        if (action == SETUP_DETECT) {
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
            if (isolated) setup::CheckIsolatedPath(paths, setup::Join(options.directory, setup::LauncherName));
            automation::UserLock operation;
            const DWORD locked = operation.AcquireForSetup(paths, isolated);
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
            const auto result = setup::Apply(paths, options, {static_cast<const BYTE*>(payload), SizeofResource(module, resource)}, target, isolated);
            const auto outcome = DescribeCommit(result.commit);
            *commit = outcome.code; // No allocation between Apply and publishing its outcome.
            auto tasks = Json::Array();
            auto warnings = Json::Array();
            std::wstring detail = result.message;
            PCWSTR message = outcome.message;
            if (result.commit == setup::CommitState::Committed) {
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
            response[L"commit"] = Json(outcome.name);
            response[L"message"] = Json(message); response[L"detail"] = Json(detail);
            response[L"tasks"] = std::move(tasks); response[L"warnings"] = std::move(warnings);
            response[L"conflictDirectory"] = Json(result.conflictDirectory);
            response[L"nativeError"] = Json::Number(result.code);
            response[L"options"] = Options(options);
        } else if (action == SETUP_LIST_PROGRAMS) {
            request.Fields({});
            response = Programs(paths, isolated);
        } else if (action == SETUP_DIRECTORY || action == SETUP_ADD_PROGRAM) {
            const bool program = action == SETUP_ADD_PROGRAM;
            request.Fields({L"initial", L"testSelection"});
            std::wstring selected;
            const auto initial = request.Has(L"initial") ? request.At(L"initial").String() : paths.defaultDirectory;
            if (request.Has(L"testSelection")) {
                if (!isolated) throw automation::Invalid("test-selection-disabled");
                selected = request.At(L"testSelection").String();
            } else Require(setup::ui::ChoosePath(reinterpret_cast<HWND>(owner), program, initial, selected));
            if (isolated && !selected.empty()) {
                setup::CheckIsolatedPath(paths, program ? selected : setup::Join(selected, setup::LauncherName));
            }
            if (program && !selected.empty()) {
                response[L"entry"] = Program(selected);
                response[L"path"] = Json(selected);
                automation::UserLock operation;
                const DWORD locked = operation.AcquireForSetup(paths, isolated);
                if (locked) throw setup::Result{locked, l10n::Text("error.busy")};
                if (isolated) {
                    const auto folder = setup::Join(paths.defaultDirectory, companions::Folder);
                    setup::CheckIsolatedPath(paths, setup::CompanionShortcutPath(folder, selected));
                }
                // Complete the reply allocation before the immediately effective write.
                Deliver(response, *output);
                try { Require(setup::AddCompanionProgram(paths, selected)); }
                catch (...) { setup_bridge_free(output); throw; }
                *commit = 1;
                return ERROR_SUCCESS;
            } else response[L"entry"] = Json{};
            response[L"path"] = Json(selected);
        } else if (action == SETUP_OPEN_LOCATION) {
            request.Fields({L"location"});
            const auto& location = request.At(L"location").String(32);
            const bool programConflict = location == L"program-conflict";
            if (location != L"startup" && location != L"deployment" && location != L"windows" && location != L"conflict" && !programConflict)
                throw automation::Invalid("unknown-location");
            if (location == L"windows") {
                if (!isolated) Require(setup::OpenStartupSettings(reinterpret_cast<HWND>(owner)));
            } else if (!isolated || location == L"conflict" || programConflict) {
                automation::UserLock operation;
                const DWORD locked = operation.AcquireForSetup(paths, isolated);
                if (locked) throw setup::Result{locked, l10n::Text("error.busy")};
                std::wstring folder;
                if (programConflict) {
                    folder = setup::Join(paths.defaultDirectory, companions::Folder);
                    automation::CheckPlainPath(folder);
                    const auto attributes = GetFileAttributesW(folder.c_str());
                    if (attributes == INVALID_FILE_ATTRIBUTES) throw automation::IoError(GetLastError());
                    if (!(attributes & FILE_ATTRIBUTE_DIRECTORY)) throw automation::IoError(ERROR_DIRECTORY);
                } else if (location == L"startup") Require(setup::EnsureCompanionFolder(paths, folder));
                else {
                    setup::Loaded loaded;
                    const auto status = setup::Load(paths, loaded);
                    if (location == L"conflict") {
                        if (status) throw automation::IoError(ERROR_NOT_FOUND);
                        if (status.conflictDirectory.empty()) throw status;
                        folder = status.conflictDirectory;
                    } else { Require(status); folder = loaded.options.directory; }
                }
                if (!isolated) {
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
            const auto& language = request.At(L"language").String(LOCALE_NAME_MAX_LENGTH - 1);
            if (language == L"auto") l10n::UseSystem();
            else if (!l10n::Select(language)) throw automation::Invalid("unknown-language");
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
        try { DeliverError(result, *output); } catch (...) {}
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

extern "C" uint32_t setup_bridge_acquire_gui(const uint16_t* windowClass, void** session, void** user) noexcept {
    if (!windowClass || !*windowClass || !session || !user || session == user) return ERROR_INVALID_PARAMETER;
    return setup::AcquireGuiInstance(reinterpret_cast<PCWSTR>(windowClass), *session, *user);
}

extern "C" int32_t setup_bridge_compatibility(const uint8_t* pathBytes, size_t size, const uint16_t* windowClass) noexcept {
    if (!windowClass || !*windowClass) return ERROR_INVALID_PARAMETER;
    try {
        setup::Paths paths;
        if (pathBytes || size) {
            const auto status = ResolvePaths(pathBytes, size, paths);
            if (!status) return static_cast<int32_t>(status.code);
        }
        return setup::RunNativeSetup(GetModuleHandleW(nullptr), SW_SHOWNORMAL, true, reinterpret_cast<PCWSTR>(windowClass),
            pathBytes ? &paths : nullptr);
    } catch (const std::bad_alloc&) { return ERROR_NOT_ENOUGH_MEMORY; }
    catch (...) { return ERROR_INVALID_DATA; }
}

extern "C" void setup_bridge_show_bootstrap_error(uint32_t compatibilityFailed) noexcept {
    MessageBoxW(nullptr, l10n::Text(compatibilityFailed ? "setup.error.compatibility" : "setup.error.webviewstart"),
        l10n::Text("setup.error.open"), MB_OK | MB_ICONERROR);
}
