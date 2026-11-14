// SPDX-License-Identifier: GPL-3.0-only
#include "CompanionRunner.h"
#include "AppVersion.h"
#include <shlobj.h>
#include <bcrypt.h>
#include <sddl.h>
#include <algorithm>
#include <memory>
#include <set>

namespace chain {
using automation::Invalid;
using automation::Handle;
namespace {
constexpr wchar_t Guard[] = L"CHATGPT_PROXY_LAUNCHER_COMPANION";
std::wstring Random() {
    BYTE bytes[32]{};
    if (BCryptGenRandom(nullptr, bytes, sizeof(bytes), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) throw Invalid("random-unavailable");
    constexpr wchar_t hex[] = L"0123456789abcdef"; std::wstring out;
    for (const auto b : bytes) { out += hex[b >> 4]; out += hex[b & 15]; } return out;
}
std::wstring Sid(HANDLE process) {
    Handle token; if (!OpenProcessToken(process, TOKEN_QUERY, &token.value)) throw automation::IoError(GetLastError());
    DWORD n = 0; GetTokenInformation(token.value, TokenUser, nullptr, 0, &n); std::vector<BYTE> bytes(n);
    if (!n || !GetTokenInformation(token.value, TokenUser, bytes.data(), n, &n)) throw automation::IoError(GetLastError());
    LPWSTR text = nullptr;
    if (!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(bytes.data())->User.Sid, &text)) throw automation::IoError(GetLastError());
    std::wstring result(text); LocalFree(text); return result;
}
struct AttemptFiles {
    std::wstring folder, context, reply;
    bool owned = false;
    void Create(const std::wstring& token) {
        wchar_t temp[MAX_PATH]{}; const DWORD n = GetTempPathW(MAX_PATH, temp);
        if (!n || n >= MAX_PATH) throw Invalid("temporary-path-unavailable");
        folder = Join(temp, L"ChatGPTLauncher-chain-" + token); automation::CheckPlainPath(folder);
        const auto dacl = L"D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;" + Sid(GetCurrentProcess()) + L")";
        PSECURITY_DESCRIPTOR sd = nullptr;
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(dacl.c_str(), SDDL_REVISION_1, &sd, nullptr)) throw automation::IoError(GetLastError());
        SECURITY_ATTRIBUTES sa{sizeof(sa), sd, FALSE}; const BOOL created = CreateDirectoryW(folder.c_str(), &sa); const DWORD error = GetLastError(); LocalFree(sd);
        if (!created) throw automation::IoError(error);
        owned = true; context = Join(folder, L"context.json"); reply = Join(folder, L"reply.json");
    }
    ~AttemptFiles() {
        if (!owned) return;
        try { automation::CheckPlainPath(folder); } catch (...) { return; }
        DeleteFileW(context.c_str()); DeleteFileW(reply.c_str()); DeleteFileW((reply + L".tmp").c_str()); RemoveDirectoryW(folder.c_str());
    }
};
struct Watch {
    HANDLE value = INVALID_HANDLE_VALUE;
    ~Watch() { if (value != INVALID_HANDLE_VALUE) FindCloseChangeNotification(value); }
};
struct AttributeList {
    std::vector<BYTE> bytes;
    PPROC_THREAD_ATTRIBUTE_LIST value = nullptr;
    HANDLE jobValue = nullptr;
    void Job(HANDLE job) {
        SIZE_T size = 0; InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
        if (!size) throw Invalid("tracking-unavailable"); bytes.resize(size);
        value = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(bytes.data());
        if (!InitializeProcThreadAttributeList(value, 1, 0, &size)) { value = nullptr; throw Invalid("tracking-unavailable"); }
        jobValue = job;
        if (!UpdateProcThreadAttribute(value, 0, PROC_THREAD_ATTRIBUTE_JOB_LIST, &jobValue, sizeof(jobValue), nullptr, nullptr)) throw Invalid("tracking-unavailable");
    }
    ~AttributeList() { if (value) DeleteProcThreadAttributeList(value); }
};
struct CaseLess { bool operator()(const std::wstring& a, const std::wstring& b) const { return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_LESS_THAN; } };
std::vector<wchar_t> Environment(const std::map<std::wstring, std::wstring>& values) {
    LPWCH raw = GetEnvironmentStringsW(); if (!raw) throw automation::IoError(GetLastError());
    struct Env { LPWCH p; ~Env() { FreeEnvironmentStringsW(p); } } env{raw};
    std::map<std::wstring, std::wstring, CaseLess> items;
    for (PCWSTR p = raw; *p; p += wcslen(p) + 1) {
        const std::wstring item(p); const auto sep = item.find(L'=', 1);
        if (sep != item.npos) items[item.substr(0, sep)] = item.substr(sep + 1);
    }
    for (const auto& v : values) items[v.first] = v.second;
    std::vector<wchar_t> out;
    for (const auto& item : items) {
        const auto text = item.first + L"=" + item.second; out.insert(out.end(), text.begin(), text.end()); out.push_back(0);
    }
    out.push_back(0); return out;
}
std::wstring DeadlineUtc(DWORD remaining) {
    FILETIME now{}; GetSystemTimeAsFileTime(&now);
    ULARGE_INTEGER ticks{}; ticks.LowPart = now.dwLowDateTime; ticks.HighPart = now.dwHighDateTime; ticks.QuadPart += static_cast<ULONGLONG>(remaining) * 10000;
    FILETIME future{ticks.LowPart, ticks.HighPart}; SYSTEMTIME utc{}; FileTimeToSystemTime(&future, &utc);
    wchar_t out[40]{}; swprintf_s(out, L"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ", utc.wYear, utc.wMonth, utc.wDay, utc.wHour, utc.wMinute, utc.wSecond, utc.wMilliseconds); return out;
}
bool EmptyJob(HANDLE job) {
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION info{};
    if (!QueryInformationJobObject(job, JobObjectBasicAccountingInformation, &info, sizeof(info), nullptr)) throw Invalid("tracking-unknown");
    return info.ActiveProcesses == 0;
}
Json Item(const std::wstring& id, const wchar_t* status, const std::wstring& code = L"", unsigned attempts = 0, ULONGLONG elapsed = 0) {
    auto j = Json::Object(); j[L"id"] = Json(id); j[L"status"] = Json(status); j[L"code"] = Json(code);
    j[L"attempts"] = Json::Number(attempts); j[L"elapsedMs"] = Json::Number(static_cast<long long>(elapsed)); return j;
}
struct AttemptResult { std::wstring status = L"failed", code; bool retryable = false, empty = false; };
bool Transient(DWORD code) { return code == ERROR_NOT_ENOUGH_MEMORY || code == ERROR_OUTOFMEMORY || code == ERROR_NO_SYSTEM_RESOURCES || code == ERROR_RETRY; }
bool DirectoryExists(const std::wstring& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        const DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) return false;
        throw automation::IoError(error);
    }
    if (!(attributes & FILE_ATTRIBUTE_DIRECTORY) || (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) throw Invalid("invalid-startup-directory");
    return true;
}
AttemptResult Attempt(const Entry& e, const RunOptions& options, const Json& info, const std::wstring& runId, unsigned attempt, ULONGLONG globalDeadline, std::map<companions::Kind, std::wstring>& hosts) {
    const auto began = GetTickCount64();
    const auto deadline = std::min(globalDeadline, began + e.timeout);
    const auto token = Random(), attemptId = Random(); AttemptFiles files; files.Create(token);
    auto context = Json::Object(); context[L"schemaVersion"] = Json::Number(1); context[L"protocolVersion"] = Json::Number(1);
    context[L"runId"] = Json(runId); context[L"attemptId"] = Json(attemptId); context[L"token"] = Json(token);
    context[L"deadlineUtc"] = Json(DeadlineUtc(static_cast<DWORD>(deadline > began ? deadline - began : 0)));
    auto launcher = Json::Object(); launcher[L"path"] = Json(options.launcher); launcher[L"version"] = Json(automation::FromUtf8(APP_VERSION_TEXT));
    launcher[L"deploymentState"] = Json(L"standalone");
    if (info.At(L"state").String() == L"managed" && automation::SamePath(info.At(L"launcherPath").String(), options.launcher)) launcher[L"deploymentState"] = Json(L"managed");
    else if (info.At(L"state").String() == L"unknown") launcher[L"deploymentState"] = Json(L"unknown");
    context[L"launcher"] = launcher;
    auto entry = Json::Object(); entry[L"id"] = Json(e.id); entry[L"version"] = Json(e.version); entry[L"attempt"] = Json::Number(attempt); context[L"entry"] = entry;
    auto input = Json::Object(); input[L"arguments"] = Json::Array(); for (const auto& a : e.arguments) input[L"arguments"].array.emplace_back(a);
    input[L"config"] = e.config; context[L"input"] = input;
    if (e.signal) context[L"replyPath"] = Json(files.reply);
    auto chat = Json::Object(); chat[L"processCreated"] = Json(true); chat[L"readiness"] = Json(L"unknown"); context[L"chatgpt"] = chat;
    const auto bytes = automation::WriteJson(context); if (bytes.size() > 65536) throw Invalid("context-too-large");
    {
        Handle file(CreateFileW(files.context.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (!file) throw automation::IoError(GetLastError()); automation::WriteBytes(file.value, bytes);
    }
    Watch watch;
    if (e.signal) { watch.value = FindFirstChangeNotificationW(files.folder.c_str(), FALSE, FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE); if (watch.value == INVALID_HANDLE_VALUE) throw Invalid("reply-watch-unavailable"); }
    std::wstring program = e.target, tail;
    if (e.kind != companions::Kind::Exe) {
        if (!e.host.empty()) program = e.host;
        else {
            auto found = hosts.find(e.kind);
            if (found == hosts.end()) found = hosts.emplace(e.kind, companions::FindHost(e.kind)).first;
            program = found->second;
        }
        if (program.empty()) return {L"failed", L"runtime-missing", false, true};
        const wchar_t* prefix = e.kind == companions::Kind::PowerShell ? L"-NoLogo -NoProfile -File " : e.kind == companions::Kind::Wsh ? L"//Nologo " : e.kind == companions::Kind::Jar ? L"-jar " : L"";
        tail = prefix + automation::Quote(e.target);
    }
    std::map<std::wstring, std::wstring> variables{{Guard, L"1"}, {L"CGL_CHAIN_PROTOCOL", L"1"}, {L"CGL_CONTEXT_FILE", files.context},
        {L"CGL_LAUNCHER_PATH", options.launcher}, {L"CGL_LAUNCHER_VERSION", automation::FromUtf8(APP_VERSION_TEXT)}, {L"CGL_RUN_ID", runId}, {L"CGL_ENTRY_ID", e.id}, {L"CGL_ATTEMPT", std::to_wstring(attempt)}};
    if (e.kind == companions::Kind::Batch) { variables[L"CHATGPT_PROXY_LAUNCHER_BATCH_PATH"] = e.target; tail = L"/d /v:off /s /c \"\"%CHATGPT_PROXY_LAUNCHER_BATCH_PATH%\"\""; }
    else for (const auto& arg : e.arguments) { if (!tail.empty()) tail += L' '; tail += automation::Quote(arg); }
    std::wstring command = automation::Quote(program) + (tail.empty() ? L"" : L" " + tail);
    if (command.size() + 1 > 32767) throw Invalid("command-too-long");
    automation::CheckPlainPath(e.target); automation::CheckPlainPath(e.directory);
    Handle job; AttributeList attributes; STARTUPINFOEXW sx{}; sx.StartupInfo.cb = sizeof(STARTUPINFOW);
    DWORD flags = CREATE_UNICODE_ENVIRONMENT;
    if (e.safe) {
        job.value = CreateJobObjectW(nullptr, nullptr); if (!job) throw Invalid("tracking-unavailable");
        attributes.Job(job.value); sx.lpAttributeList = attributes.value; sx.StartupInfo.cb = sizeof(sx); flags |= EXTENDED_STARTUPINFO_PRESENT;
    }
    if (e.hidden) { flags |= CREATE_NO_WINDOW; sx.StartupInfo.dwFlags = STARTF_USESHOWWINDOW; sx.StartupInfo.wShowWindow = SW_HIDE; }
    auto env = Environment(variables); PROCESS_INFORMATION pi{};
    if (GetTickCount64() >= deadline) return {L"skipped-budget", L"budget-exhausted", false, true};
    if (!CreateProcessW(program.c_str(), command.data(), nullptr, nullptr, FALSE, flags, env.data(), e.directory.c_str(), &sx.StartupInfo, &pi)) {
        const DWORD error = GetLastError(); return {L"failed", L"create-process-" + std::to_wstring(error), Transient(error), true};
    }
    Handle process(pi.hProcess), thread(pi.hThread), existing;
    bool acknowledged = false, failed = false, retryable = false;
    std::wstring code, success = L"ready"; ULONGLONG observedAt = 0;
    for (;;) {
        auto now = GetTickCount64();
        const DWORD processWait = WaitForSingleObject(process.value, 0);
        if (processWait == WAIT_FAILED) return {L"tracking-unknown", L"process-query-failed", false, false};
        const bool exited = processWait == WAIT_OBJECT_0;
        if (e.signal && !acknowledged && !failed && GetFileAttributesW(files.reply.c_str()) != INVALID_FILE_ATTRIBUTES) {
            try {
                const auto reply = automation::ParseJson(automation::ReadBytes(files.reply, 4096), 4096);
                reply.Fields({L"schemaVersion", L"runId", L"attemptId", L"token", L"status", L"readyProcessId", L"readyProcessCreationTime", L"errorCode", L"retryable", L"message"});
                if (reply.At(L"schemaVersion").Integer(1, 1) != 1 || reply.At(L"runId").String() != runId || reply.At(L"attemptId").String() != attemptId || reply.At(L"token").String() != token || now >= deadline) throw Invalid("invalid-reply");
                const auto status = reply.At(L"status").String();
                if (status == L"failed") {
                    reply.Fields({L"schemaVersion", L"runId", L"attemptId", L"token", L"status", L"errorCode", L"retryable", L"message"});
                    code = reply.At(L"errorCode").String(64); retryable = reply.At(L"retryable").Bool();
                    if (code.empty() || code[0] < L'a' || code[0] > L'z' || code.find_first_not_of(L"abcdefghijklmnopqrstuvwxyz0123456789-") != code.npos) throw Invalid("invalid-error-code");
                    if (code == L"invalid-config" || code == L"invalid-arguments" || code == L"permission-denied" || code == L"runtime-missing" || code == L"unsupported-capability") retryable = false;
                    if (reply.Has(L"message")) reply.At(L"message").String(256);
                    failed = true;
                } else if (status == L"ready" || status == L"already-running") {
                    if (status == L"already-running") {
                        reply.Fields({L"schemaVersion", L"runId", L"attemptId", L"token", L"status", L"readyProcessId", L"readyProcessCreationTime"});
                        const DWORD pid = static_cast<DWORD>(reply.At(L"readyProcessId").Integer(1, MAXDWORD));
                        const auto creation = reply.At(L"readyProcessCreationTime").String(16);
                        if (creation.size() != 16 || creation.find_first_not_of(L"0123456789abcdefABCDEF") != creation.npos) throw Invalid("invalid-ready-process");
                        Handle candidate(OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
                        if (!candidate) throw Invalid("ready-process-unavailable");
                        FILETIME birth{}, exit{}, kernel{}, user{}; DWORD session = 0, ours = 0;
                        if (!GetProcessTimes(candidate.value, &birth, &exit, &kernel, &user) || !ProcessIdToSessionId(pid, &session) || !ProcessIdToSessionId(GetCurrentProcessId(), &ours)
                            || session != ours || Sid(candidate.value) != Sid(GetCurrentProcess())) throw Invalid("ready-process-mismatch");
                        const ULONGLONG stamp = (static_cast<ULONGLONG>(birth.dwHighDateTime) << 32) | birth.dwLowDateTime;
                        if (stamp != _wcstoui64(creation.c_str(), nullptr, 16)) throw Invalid("ready-process-reused");
                        existing.Reset(candidate.value); candidate.value = INVALID_HANDLE_VALUE;
                        success = status;
                    } else reply.Fields({L"schemaVersion", L"runId", L"attemptId", L"token", L"status"});
                    acknowledged = true; observedAt = GetTickCount64();
                } else throw Invalid("invalid-reply-status");
            } catch (...) { /* Invalid or unfinished replies cannot confirm readiness. */ }
        }
        now = GetTickCount64();
        if (acknowledged) {
            HANDLE ready = existing ? existing.value : process.value;
            const DWORD readyWait = WaitForSingleObject(ready, 0);
            if (readyWait != WAIT_TIMEOUT) { failed = true; acknowledged = false; retryable = true; code = L"exited-after-ready"; }
            else if (now - observedAt >= 1000 && now <= deadline) return {success, L"", false, false};
        }
        bool taskChildren = false;
        if (!e.signal && exited) {
            DWORD exitCode = 0; if (!GetExitCodeProcess(process.value, &exitCode)) return {L"tracking-unknown", L"exit-query-failed", false, false};
            const bool successCode = std::find(e.exitCodes.begin(), e.exitCodes.end(), exitCode) != e.exitCodes.end();
            const bool empty = !job || EmptyJob(job.value);
            if (successCode && empty) return {L"completed", L"", false, true};
            // A signalled process handle can precede the Job accounting update.
            // Allow the process family to drain within this attempt's deadline.
            if (successCode) taskChildren = true;
            else { failed = true; retryable = true; code = L"exit-code-" + std::to_wstring(exitCode); }
        }
        if (e.signal && exited && !acknowledged && !failed) { failed = true; retryable = true; code = L"exited-before-ready"; }
        const bool empty = exited && (!job || EmptyJob(job.value));
        if (failed && (!retryable || empty)) return {L"failed", code, retryable, empty};
        if (now >= deadline) return {empty ? L"failed" : L"timeout-running", taskChildren ? L"task-left-children" : code.empty() ? L"ready-timeout" : code, empty && retryable, empty};
        HANDLE waits[3]{}; DWORD count = 0;
        if (!exited) waits[count++] = process.value;
        if (watch.value != INVALID_HANDLE_VALUE) waits[count++] = watch.value;
        if (existing && WaitForSingleObject(existing.value, 0) == WAIT_TIMEOUT) waits[count++] = existing.value;
        DWORD delay = static_cast<DWORD>(std::min<ULONGLONG>(deadline - now, acknowledged ? 1000 - std::min<ULONGLONG>(1000, now - observedAt) : exited ? 250 : MAXDWORD));
        if (!count) Sleep(delay);
        else {
            const DWORD wait = WaitForMultipleObjects(count, waits, FALSE, delay);
            if (wait == WAIT_FAILED) return {L"tracking-unknown", L"wait-failed", false, false};
            if (watch.value != INVALID_HANDLE_VALUE && WaitForSingleObject(watch.value, 0) == WAIT_OBJECT_0 && !FindNextChangeNotification(watch.value)) return {L"failed", L"reply-watch-failed", false, false};
        }
    }
}
thread_local Json* legacyResults = nullptr;
thread_local const std::vector<Record>* activeRecords = nullptr;
bool AllowLegacy(PCWSTR target) {
    if (activeRecords) for (const auto& r : *activeRecords)
        if (r.error.empty() && r.entry.enabled && automation::SamePath(r.entry.target, target)) return false;
    return true;
}
void LegacyResult(PCWSTR file, DWORD error) {
    if (!legacyResults) return;
    PCWSTR leaf = wcsrchr(file, L'\\'); leaf = leaf ? leaf + 1 : file;
    legacyResults->array.push_back(Item(L"legacy:" + std::wstring(leaf), error == ERROR_TIMEOUT ? L"skipped-budget" : error ? L"failed" : L"started-unverified", error == ERROR_TIMEOUT ? L"budget-exhausted" : error == ERROR_ALREADY_EXISTS ? L"legacy-entry-conflict" : error ? L"start-failed" : L"", error == ERROR_ALREADY_EXISTS ? 0 : 1));
}
bool Succeeded(const Json& item) { const auto& s = item.At(L"status").String(); return s == L"ready" || s == L"already-running" || s == L"completed" || s == L"started-unverified" || s == L"disabled"; }
}
std::wstring RootDirectory() {
    PWSTR path = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DONT_VERIFY, nullptr, &path))) throw Invalid("local-data-unavailable");
    const auto root = Join(path, companions::Root); CoTaskMemFree(path); return root;
}
Json Run(const RunOptions& options, PCWSTR chat, companions::Start legacyStart) noexcept {
    auto report = Json::Object(); report[L"entries"] = Json::Array(); report[L"chainStatus"] = Json(L"completed"); report[L"exitCode"] = Json::Number(0);
    bool notify = options.feedback != 0;
    try {
        wchar_t guard[2]{};
        if (GetEnvironmentVariableW(Guard, guard, 2)) { report[L"chainStatus"] = Json(L"not-started"); report[L"exitCode"] = Json::Number(10); return report; }
        const auto folder = Join(options.root, L"CompanionRegistry"), legacy = Join(options.root, companions::Folder);
        if (!DirectoryExists(folder) && !DirectoryExists(legacy)) return report;
        automation::UserLock lock;
        const DWORD locked = lock.Acquire(Join(options.root, L"chain-start"), 0);
        if (locked) { report[L"chainStatus"] = Json(L"skipped-busy"); report[L"exitCode"] = Json::Number(10); return report; }
        DWORD budget = 60000; std::wstring policyError;
        const auto policyFile = Join(options.root, L"companion-policy.json");
        if (GetFileAttributesW(policyFile.c_str()) != INVALID_FILE_ATTRIBUTES) {
            try {
                const auto policy = automation::ParseJson(automation::ReadBytes(policyFile, 4096), 4096); policy.Fields({L"schemaVersion", L"budgetMs", L"feedback"});
                if (policy.At(L"schemaVersion").Integer(1, 1) != 1) throw Invalid("invalid-policy-version");
                budget = static_cast<DWORD>(policy.At(L"budgetMs").Integer(1000, 120000));
                const auto feedback = policy.At(L"feedback").String(); if (feedback != L"quiet" && feedback != L"notify") throw Invalid("invalid-feedback"); notify = feedback == L"notify";
            } catch (...) { policyError = L"invalid-policy"; }
        }
        if (options.feedback >= 0) notify = options.feedback != 0;
        const auto deadline = GetTickCount64() + budget;
        auto& items = report[L"entries"];
        std::vector<Record> records; std::vector<size_t> order;
        if (policyError.empty()) {
            try { records = ReadRegistry(options.root); if (!SortEntries(records, order)) policyError = L"invalid-graph"; }
            catch (const Invalid& ex) { policyError = automation::FromUtf8(ex.what()); }
            catch (...) { policyError = L"registry-read-failed"; }
        }
        legacyResults = &items; activeRecords = policyError.empty() ? &records : nullptr;
        try { companions::RunFolder(legacy, chat, legacyStart, LegacyResult, deadline, AllowLegacy); } catch (...) { items.array.push_back(Item(L"legacy", L"failed", L"legacy-read-failed")); }
        legacyResults = nullptr; activeRecords = nullptr;
        if (!policyError.empty()) items.array.push_back(Item(L"registry", L"failed", policyError));
        else if (!records.empty()) {
            RunOptions execution = options;
            if (execution.launcher.empty()) {
                execution.launcher.resize(32768); const DWORD n = GetModuleFileNameW(nullptr, execution.launcher.data(), static_cast<DWORD>(execution.launcher.size()));
                if (!n || n >= execution.launcher.size()) throw Invalid("launcher-path-unavailable"); execution.launcher.resize(n);
            }
            const auto deployment = DeploymentInfo(options.root);
            const auto runId = Random(); std::map<std::wstring, bool> success; std::map<companions::Kind, std::wstring> hosts;
            for (const auto i : order) {
                const auto& r = records[i]; const auto& e = r.entry;
                Json result;
                if (!r.error.empty()) result = Item(r.id, L"failed", r.error);
                else if (!e.enabled) result = Item(r.id, L"disabled");
                else {
                    bool dependencies = true;
                    for (const auto& d : e.dependencies) {
                        const auto dep = std::find_if(records.begin(), records.end(), [&](const Record& candidate) { return candidate.id == d.id; });
                        if (dep == records.end() || !success[d.id] || (!d.minimumVersion.empty() && CompareVersion(dep->entry.version, d.minimumVersion) < 0)) dependencies = false;
                    }
                    if (!dependencies) result = Item(r.id, L"skipped-dependency", L"dependency-unavailable");
                    else if (GetTickCount64() >= deadline) result = Item(r.id, L"skipped-budget", L"budget-exhausted");
                    else {
                        const auto began = GetTickCount64(); AttemptResult outcome; unsigned tries = 0;
                        for (unsigned attempt = 1; attempt <= e.attempts; ++attempt) {
                            ++tries;
                            try { outcome = Attempt(e, execution, deployment, runId, attempt, deadline, hosts); }
                            catch (const Invalid& ex) { outcome = {std::string(ex.what()) == "tracking-unknown" ? L"tracking-unknown" : L"failed", automation::FromUtf8(ex.what()), false, false}; }
                            catch (...) { outcome = {L"failed", L"launch-error", false, false}; }
                            if (!e.safe || !outcome.retryable || !outcome.empty || attempt == e.attempts || GetTickCount64() + e.delay >= deadline) break;
                            Sleep(e.delay);
                        }
                        result = Item(r.id, outcome.status.c_str(), outcome.code, tries, GetTickCount64() - began);
                    }
                }
                success[r.id] = Succeeded(result) && result.At(L"status").String() != L"disabled"; items.array.push_back(result);
            }
        }
        if (std::any_of(items.array.begin(), items.array.end(), [](const Json& item) { return !Succeeded(item); })) {
            report[L"exitCode"] = Json::Number(10); report[L"chainStatus"] = Json(L"partial"); if (notify) NotifyFailures(report);
        }
    } catch (...) {
        legacyResults = nullptr; activeRecords = nullptr; report[L"chainStatus"] = Json(L"partial"); report[L"exitCode"] = Json::Number(10);
        report[L"entries"].array.push_back(Item(L"registry", L"failed", L"chain-read-failed"));
        if (notify) NotifyFailures(report);
    }
    return report;
}
}
