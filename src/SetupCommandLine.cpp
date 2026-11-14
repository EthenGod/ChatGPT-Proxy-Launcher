// SPDX-License-Identifier: GPL-3.0-only
#include "SetupCommandLine.h"
#include "NativeMemory.h"
#include "SetupResources.h"
#include "AppVersion.h"
#include "CompanionManifest.h"
#include "DeploymentDiscovery.h"
#include <shellapi.h>
#include <objbase.h>
#include <set>

namespace setup {
using automation::Json;
using automation::Invalid;
namespace rules = automation::contract;
namespace {
Json NewResult(const std::wstring& operation) {
    auto j = Json::Object(); j[L"schemaVersion"] = Json::Number(rules::SetupApi); j[L"operation"] = Json(operation);
    j[L"status"] = Json(L"success"); j[L"exitCode"] = Json::Number(0); j[L"code"] = Json(L"ok");
    j[L"committed"] = Json(false); j[L"stage"] = Json(L"preflight"); j[L"nativeError"] = Json::Number(0);
    j[L"warnings"] = Json::Array(); j[L"actionsRequired"] = Json::Array(); j[L"data"] = Json::Object(); return j;
}
void Failure(Json& j, int exitCode, const std::wstring& code, DWORD native = 0) {
    j[L"status"] = Json(L"error"); j[L"exitCode"] = Json::Number(exitCode); j[L"code"] = Json(code);
    j[L"nativeError"] = Json::Number(native);
}
void FromCore(Json& j, const Result& r) {
    int exitCode = r.commit == CommitState::Incomplete ? 41 : r.commit == CommitState::RolledBack ? 40 :
        !r.conflictDirectory.empty() ? 30 : r.code == ERROR_BUSY ? 31 :
        r.code == ERROR_INVALID_PARAMETER || r.code == ERROR_INVALID_DATA || r.code == ERROR_BAD_PATHNAME || r.code == ERROR_FILENAME_EXCED_RANGE ? 20 : 50;
    Failure(j, exitCode, exitCode == 30 ? L"ownership-conflict" : exitCode == 31 ? L"busy" : exitCode == 41 ? L"rollback-incomplete" : exitCode == 40 ? L"commit-failed" : exitCode == 20 ? L"invalid-request" : L"preflight-failed", r.code);
    if (exitCode == 41) j[L"committed"] = Json{};
    if (exitCode == 40 || exitCode == 41) j[L"stage"] = Json(L"rollback");
}
bool Choice(const std::wstring& value) { if (value != L"on" && value != L"off") throw Invalid("expected-on-off"); return value == L"on"; }
Json ParseProxy(std::wstring value) {
    auto j = Json::Object(); j[L"enabled"] = Json(value != L"off"); if (value == L"off") return j;
    const auto scheme = value.find(L"://"); if (scheme == value.npos) throw Invalid("invalid-proxy-url");
    j[L"protocol"] = Json(value.substr(0, scheme)); value.erase(0, scheme + 3);
    if (!value.empty() && value.back() == L'/') value.pop_back();
    if (value.find_first_of(L"/?#@\\") != value.npos) throw Invalid("invalid-proxy-url");
    const auto colon = value.find_last_of(L':'); if (colon == value.npos || colon + 1 == value.size()) throw Invalid("missing-proxy-port");
    auto host = value.substr(0, colon); const auto port = value.substr(colon + 1);
    if (host.size() > 2 && host.front() == L'[' && host.back() == L']') host = host.substr(1, host.size() - 2);
    else if (host.find(L':') != host.npos) throw Invalid("invalid-ipv6-url");
    if (port.size() > 5 || port.find_first_not_of(L"0123456789") != port.npos) throw Invalid("invalid-proxy-port");
    j[L"host"] = Json(host); j[L"port"] = Json::Number(wcstoul(port.c_str(), nullptr, 10)); return j;
}
void CheckResultLocation(const AutomationCommand& c, const Paths& p, const Options* options = nullptr) {
    if (c.resultPath.empty()) throw Invalid("missing-result-path");
    const auto output = automation::FullPath(c.resultPath);
    if (output.size() < 5 || _wcsicmp(output.c_str() + output.size() - 5, L".json")) throw Invalid("result-must-be-json-file");
    const auto registry = Join(p.defaultDirectory, rules::RegistryFolder) + L"\\";
    if (output.size() >= registry.size() && automation::SamePath(output.substr(0, registry.size()), registry)) throw Invalid("result-is-registration");
    for (const auto& path : {c.requestPath, c.manifestPath, p.settingsLink, p.startupLink, p.desktopLink, p.codexEnvFile,
            Join(p.defaultDirectory, rules::DiscoveryFile), Join(p.defaultDirectory, rules::PolicyFile), Join(p.defaultDirectory, LauncherName)}) {
        if (!path.empty() && automation::SamePath(output, automation::FullPath(path))) throw Invalid("result-is-managed-file");
    }
    if (options && automation::SamePath(output, automation::FullPath(Join(options->directory, LauncherName)))) throw Invalid("result-is-launcher");
}
Json PlanData(const Options& o, const Paths& paths, const Json& request) {
    auto j = Json::Object(); j[L"directory"] = Json(o.directory); j[L"startup"] = Json(o.startup); j[L"desktop"] = Json(o.desktop);
    j[L"arguments"] = Json(L""); std::wstring args; const auto result = BuildArguments(o, args);
    if (!result) throw Invalid("invalid-arguments");
    j[L"arguments"] = Json(args); j[L"writesCodexEnv"] = Json(o.codexEnv && o.proxy);
    j[L"changes"] = Json::Array();
    auto change = [&](PCWSTR item, PCWSTR action, const std::wstring& path) {
        auto c = Json::Object(); c[L"item"] = Json(item); c[L"action"] = Json(action); c[L"path"] = Json(path);
        j[L"changes"].array.push_back(c);
    };
    change(L"launcher", L"save", Join(o.directory, LauncherName));
    change(L"settings", L"save", paths.settingsLink);
    change(L"startup", o.startup ? L"save" : L"remove-if-managed", paths.startupLink);
    change(L"desktop", o.desktop ? L"save" : L"remove-if-managed", paths.desktopLink);
    change(L"discovery", L"save", Join(paths.defaultDirectory, rules::DiscoveryFile));
    if (o.codexEnv) change(L"codex-env", L"merge", paths.codexEnvFile);
    j[L"actions"] = request.Has(L"actions") ? request.At(L"actions") : Json::Object();
    return j;
}
}
void ParseAutomationCommand(const std::wstring& tail, AutomationCommand& command) {
    command = {};
    int count = 0; LPWSTR* raw = CommandLineToArgvW((L"setup " + tail).c_str(), &count);
    if (!raw) throw Invalid("invalid-command-line");
    native::LocalPtr<LPWSTR> ownedArguments(raw);
    std::vector<std::wstring> args(raw + 1, raw + count);
    if (args.size() < 2 || args[0] != L"--headless") throw Invalid("expected-headless-command");
    command.operation = args[1];
    size_t firstOption = 2;
    const bool companion = command.operation == L"companion";
    if (companion) {
        if (args.size() < 3) throw Invalid("missing-companion-operation");
        command.operation += L"-" + args[2]; firstOption = 3;
    }
    auto& request = command.request; request[L"schemaVersion"] = Json::Number(rules::SetupApi);
    auto settings = Json::Object(), actions = Json::Object(), extra = Json::Array();
    std::set<std::wstring> seen; bool flat = false, clearExtra = false;
    for (size_t i = firstOption; i < args.size(); ++i) {
        const auto key = args[i];
        if (key != L"--extra-arg" && !seen.insert(key).second) { if (key == L"--result") command.resultPath.clear(); throw Invalid("duplicate-option"); }
        if (companion && key == L"--replace") { command.replaceManifest = true; continue; }
        if (key == L"--clear-extra") { clearExtra = true; flat = true; continue; }
        if (i + 1 == args.size()) throw Invalid("missing-option-value");
        const auto value = args[++i];
        if (key == L"--result") { command.resultPath = automation::FullPath(value); continue; }
        if (key == L"--request") { command.requestPath = automation::FullPath(value); continue; }
        if (companion && key == L"--manifest") { command.manifestPath = automation::FullPath(value); continue; }
        if (companion && key == L"--id") { command.entryId = value; continue; }
        flat = true;
        if (key == L"--base") request[L"base"] = Json(value);
        else if (key == L"--directory") settings[L"directory"] = Json(value);
        else if (key == L"--startup") settings[L"startup"] = Json(Choice(value));
        else if (key == L"--desktop") settings[L"desktop"] = Json(Choice(value));
        else if (key == L"--proxy") settings[L"proxy"] = ParseProxy(value);
        else if (key == L"--bypass") settings[L"bypass"] = Json(value);
        else if (key == L"--extra-arg") extra.array.emplace_back(value);
        else if (key == L"--codex-env") actions[L"codexEnv"] = Json(value);
        else if (key == L"--official-startup") actions[L"officialStartup"] = Json(value);
        else throw Invalid("unknown-option");
    }
    if (command.resultPath.empty()) throw Invalid("missing-result-path");
    if (companion) {
        if (flat || clearExtra) throw Invalid("unexpected-deploy-options");
        if (command.operation == L"companion-register") {
            if (command.manifestPath.empty() || !command.requestPath.empty() || !command.entryId.empty()) throw Invalid("invalid-register-options");
        } else if (command.operation == L"companion-configure") {
            if (command.requestPath.empty() || !command.manifestPath.empty() || !command.entryId.empty() || command.replaceManifest) throw Invalid("invalid-configure-options");
        } else if (command.operation == L"companion-remove") {
            if (command.entryId.empty() || !command.requestPath.empty() || !command.manifestPath.empty() || command.replaceManifest) throw Invalid("invalid-remove-options");
        } else if (command.operation == L"companion-list") {
            if (!command.entryId.empty() || !command.requestPath.empty() || !command.manifestPath.empty() || command.replaceManifest) throw Invalid("invalid-list-options");
        } else throw Invalid("unknown-operation");
        return;
    }
    if (command.operation != L"deploy" && command.operation != L"plan" && command.operation != L"info") throw Invalid("unknown-operation");
    if (command.operation == L"info" && (flat || !command.requestPath.empty())) throw Invalid("info-has-settings");
    if (!command.requestPath.empty() && flat) throw Invalid("mixed-request-and-flags");
    if (clearExtra && !extra.array.empty()) throw Invalid("mixed-extra-options");
    if (clearExtra || !extra.array.empty()) settings[L"extraArguments"] = extra;
    if (!settings.object.empty()) request[L"settings"] = settings;
    if (!actions.object.empty()) request[L"actions"] = actions;
}
Options ResolveAutomationOptions(const Json& request, const Paths& paths, const Loaded& loaded) {
    request.Fields({L"schemaVersion", L"base", L"settings", L"actions"});
    if (request.At(L"schemaVersion").Integer(0, MAXDWORD) != rules::SetupApi) throw Invalid("unsupported-schema-version");
    const auto base = request.Has(L"base") ? request.At(L"base").String() : L"preserve";
    if (base != L"preserve" && base != L"defaults") throw Invalid("invalid-base");
    Options options;
    if (base == L"preserve" && loaded.hasSettings) {
        if (!loaded.validArguments) throw Invalid("invalid-saved-arguments");
        options = loaded.options;
    } else options.directory = paths.defaultDirectory;
    options.codexEnv = false;
    if (request.Has(L"settings")) {
        const auto& s = request.At(L"settings"); s.Fields({L"directory", L"startup", L"desktop", L"proxy", L"bypass", L"extraArguments"});
        if (s.Has(L"directory")) options.directory = s.At(L"directory").String();
        if (s.Has(L"startup")) options.startup = s.At(L"startup").Bool();
        if (s.Has(L"desktop")) options.desktop = s.At(L"desktop").Bool();
        if (s.Has(L"bypass")) options.bypass = s.At(L"bypass").String(8192);
        if (s.Has(L"proxy")) {
            const auto& p = s.At(L"proxy"); options.proxy = p.At(L"enabled").Bool();
            if (!options.proxy) p.Fields({L"enabled"});
            else {
                p.Fields({L"enabled", L"protocol", L"host", L"port"});
                options.protocol = p.At(L"protocol").String(16); options.host = p.At(L"host").String(253);
                if (options.host.find_first_of(L"[]") != options.host.npos) throw Invalid("host-must-not-have-brackets");
                if (options.host.find(L':') != options.host.npos) options.host = L"[" + options.host + L"]";
                options.port = std::to_wstring(p.At(L"port").Integer(1, 65535));
            }
        }
        if (s.Has(L"extraArguments")) {
            const auto& list = s.At(L"extraArguments");
            if (list.type != Json::Type::Array || list.array.size() > rules::Arguments) throw Invalid("invalid-argument-array");
            options.extra.clear();
            for (const auto& arg : list.array) { if (!options.extra.empty()) options.extra += L' '; options.extra += QuoteArgument(arg.String(rules::ArgumentChars)); }
        }
    }
    if (request.Has(L"actions")) {
        const auto& a = request.At(L"actions"); a.Fields({L"codexEnv", L"officialStartup"});
        if (a.Has(L"codexEnv")) {
            const auto& value = a.At(L"codexEnv").String();
            if (value != L"leave" && value != L"update") throw Invalid("invalid-codex-env-action");
            options.codexEnv = value == L"update";
        }
        if (a.Has(L"officialStartup")) {
            const auto& value = a.At(L"officialStartup").String();
            if (value != L"leave" && value != L"disable") throw Invalid("invalid-official-startup-action");
        }
    }
    if (options.codexEnv && !options.proxy) throw Invalid("codex-env-requires-proxy");
    std::wstring normalized, args;
    if (!NormalizeDirectory(options.directory, normalized) || !BuildArguments(options, args)) throw Invalid("invalid-launcher-options");
    options.directory = normalized; return options;
}
Json RunAutomation(const AutomationCommand& command, const Paths& paths, Payload payload, StartupReport (*disableStartup)()) {
    auto result = NewResult(command.operation); automation::ResultFile output;
    bool applying = false, registryCommitted = false;
    try {
        CheckResultLocation(command, paths);
        output.Reserve(command.resultPath);
        auto request = command.request;
        if (!command.requestPath.empty()) request = automation::ParseJson(automation::ReadBytes(command.requestPath));
        if (command.operation.find(L"companion-") == 0 || command.operation == L"info") {
            automation::UserLock lock; const DWORD error = lock.Acquire(paths.settingsLink);
            if (error) { Failure(result, error == ERROR_BUSY ? 31 : 50, L"busy", error); result[L"stage"] = Json(L"lock"); }
            else {
                result[L"data"] = command.operation == L"info"
                    ? automation::ReadDeploymentInfo(paths.defaultDirectory, paths.settingsLink)
                    : chain::RegistryCommand(command.operation, paths, command.manifestPath, command.entryId, request,
                        command.replaceManifest, registryCommitted, result[L"warnings"]);
                result[L"committed"] = Json(registryCommitted);
                if (!result[L"warnings"].array.empty()) { result[L"status"] = Json(L"success-with-warnings"); result[L"exitCode"] = Json::Number(10); }
            }
            try { output.Write(result); } catch (...) { Failure(result, 70, L"result-unavailable"); }
            return result;
        }
        Loaded initial; auto status = Load(paths, initial);
        if (!status) { FromCore(result, status); }
        Options options;
        if (status) options = ResolveAutomationOptions(request, paths, initial);
        CheckResultLocation(command, paths, status ? &options : nullptr);
        if (!initial.managedTarget.empty() && automation::SamePath(command.resultPath, automation::FullPath(initial.managedTarget))) throw Invalid("result-is-installed-launcher");
        if (status) {
            automation::UserLock lock;
            const DWORD locked = lock.Acquire(paths.settingsLink);
            if (locked) { Failure(result, locked == ERROR_BUSY ? 31 : 50, L"busy", locked); result[L"stage"] = Json(L"lock"); }
            else {
                Loaded loaded; status = Load(paths, loaded);
                if (!status) FromCore(result, status);
                else {
                    options = ResolveAutomationOptions(request, paths, loaded);
                    CheckResultLocation(command, paths, &options);
                    status = Preflight(paths, options, payload);
                    if (!status) FromCore(result, status);
                    else if (command.operation == L"plan") result[L"data"] = PlanData(options, paths, request);
                    else {
                        result[L"stage"] = Json(L"commit"); applying = true;
                        std::wstring target; status = Apply(paths, options, payload, target); applying = false;
                        if (!status) FromCore(result, status);
                        else {
                            result[L"committed"] = Json(true); result[L"code"] = Json(L"deployed");
                            auto data = Json::Object(); data[L"launcherPath"] = Json(target); data[L"launcherVersion"] = Json(automation::FromUtf8(APP_VERSION_TEXT)); result[L"data"] = data;
                            result[L"stage"] = Json(L"post-actions");
                            if (options.codexEnv) result[L"actionsRequired"].array.emplace_back(L"restart-chatgpt");
                            if (request.Has(L"actions") && request.At(L"actions").Has(L"officialStartup") && request.At(L"actions").At(L"officialStartup").String() == L"disable") {
                                try {
                                    const auto report = disableStartup();
                                    if (report.error || report.manifestError || report.packagedError || report.restoreError || !report.packagedTask
                                        || report.packagedDisabled + report.packagedAlreadyOff < report.packagedTasks)
                                        result[L"actionsRequired"].array.emplace_back(L"check-official-startup");
                                } catch (...) { result[L"actionsRequired"].array.emplace_back(L"check-official-startup"); }
                            }
                            if (!result[L"actionsRequired"].array.empty()) {
                                result[L"status"] = Json(L"success-with-warnings"); result[L"exitCode"] = Json::Number(10); result[L"code"] = Json(L"deployed-with-actions");
                            }
                        }
                    }
                }
            }
        }
    } catch (const Invalid& e) {
        const std::string reason = e.what();
        Failure(result, reason == "unsupported-schema-version" || reason == "unsupported-capability" ? 21 : reason == "legacy-entry-conflict" ? 30 : 20, automation::FromUtf8(reason));
    } catch (const automation::IoError& e) {
        Failure(result, !output.Ready() ? 70 : e.code == ERROR_ALREADY_EXISTS || e.code == ERROR_FILE_EXISTS ? 30 : 50,
            !output.Ready() ? L"result-unavailable" : e.code == ERROR_ALREADY_EXISTS || e.code == ERROR_FILE_EXISTS ? L"ownership-conflict" : L"read-failed", e.code);
    } catch (...) {
        Failure(result, 50, L"internal-error", ERROR_NOT_ENOUGH_MEMORY);
    }
    if (applying) { result[L"committed"] = Json{}; Failure(result, 41, L"commit-state-unknown"); result[L"stage"] = Json(L"commit"); }
    else if (registryCommitted || (result.At(L"committed").type == Json::Type::Boolean && result.At(L"committed").Bool())) {
        result[L"committed"] = Json(true);
        if (result.At(L"status").String() == L"error") {
            result[L"status"] = Json(L"success-with-warnings"); result[L"exitCode"] = Json::Number(10); result[L"code"] = Json(L"committed-post-action-error");
        }
    }
    try {
        if (output.Ready()) output.Write(result);
        else Failure(result, 70, L"result-unavailable");
    } catch (...) { Failure(result, 70, L"result-unavailable"); }
    return result;
}
int RunSetupCommandLine(HINSTANCE instance, const std::wstring& tail) noexcept try {
    AutomationCommand command;
    try {
        ParseAutomationCommand(tail, command);
        Paths paths; const auto got = GetPaths(paths); if (!got) return 50;
        const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (FAILED(com)) return 50;
        struct Apartment { ~Apartment() { CoUninitialize(); } } apartment;
        HRSRC resource = FindResourceW(instance, MAKEINTRESOURCEW(IDR_LAUNCHER), RT_RCDATA);
        HGLOBAL loaded = resource ? LoadResource(instance, resource) : nullptr;
        Payload payload{loaded ? static_cast<const BYTE*>(LockResource(loaded)) : nullptr, resource ? SizeofResource(instance, resource) : 0};
        auto result = RunAutomation(command, paths, payload);
        return static_cast<int>(result.At(L"exitCode").Integer(0, 255));
    } catch (const Invalid& e) {
        try {
            auto result = NewResult(command.operation); Failure(result, 20, automation::FromUtf8(e.what())); result[L"stage"] = Json(L"parse");
            // No request file is opened on a command syntax error.
            if (!command.resultPath.empty() && (command.requestPath.empty() || !automation::SamePath(command.resultPath, command.requestPath))) {
                Paths paths; if (!GetPaths(paths)) return 50; CheckResultLocation(command, paths);
                automation::ResultFile file; file.Reserve(command.resultPath); file.Write(result);
            }
        } catch (...) { return 70; }
        return 20;
    } catch (...) { return 50; }
} catch (...) {
    // AutomationCommand contains JSON storage, which may fail before the inner try.
    return 50;
}
}
