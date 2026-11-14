// SPDX-License-Identifier: GPL-3.0-only
#include "../src/CompanionRunner.h"
#include "../src/LauncherAutomation.h"
#include "../src/SetupCommandLine.h"
#include <commctrl.h>
#include <shlobj.h>
#include <stdio.h>
#include <algorithm>
#include <psapi.h>
#include <thread>
#include "MemoryFault.h"
static unsigned notifications = 0, closeSignals = 0, legacyCalls = 0;
static bool rejectJob = false, rejectJobQuery = false;
static thread_local PCWSTR syntheticEnvironment = nullptr;
static LPWCH WINAPI TestEnvironmentStrings() {
    return syntheticEnvironment ? const_cast<LPWCH>(syntheticEnvironment) : GetEnvironmentStringsW();
}
static BOOL WINAPI TestFreeEnvironment(LPWCH block) {
    return block == syntheticEnvironment ? TRUE : FreeEnvironmentStringsW(block);
}
static HANDLE WINAPI TestCreateJob(LPSECURITY_ATTRIBUTES attributes, LPCWSTR name) {
    if (rejectJob) { SetLastError(ERROR_ACCESS_DENIED); return nullptr; }
    return CreateJobObjectW(attributes, name);
}
static BOOL WINAPI TestQueryJob(HANDLE job, JOBOBJECTINFOCLASS kind, LPVOID value, DWORD size, LPDWORD returned) {
    if (rejectJobQuery) { SetLastError(ERROR_ACCESS_DENIED); return FALSE; }
    return QueryInformationJobObject(job, kind, value, size, returned);
}
static LRESULT WINAPI FakeMessage(HWND, UINT msg, WPARAM button, LPARAM) { if (msg == TDM_CLICK_BUTTON && button == IDCLOSE) ++closeSignals; return 0; }
static HRESULT WINAPI FakeDialog(const TASKDIALOGCONFIG* c, int*, int*, BOOL*) {
    ++notifications;
    if (!(c->dwFlags & TDF_CALLBACK_TIMER) || !c->pfCallback) return E_INVALIDARG;
    c->pfCallback(nullptr, TDN_TIMER, 9999, 0, 0); c->pfCallback(nullptr, TDN_TIMER, 10000, 0, 0); return S_OK;
}
static BOOL WINAPI FakeUserObject(HANDLE, int, PVOID data, DWORD, LPDWORD) { static_cast<USEROBJECTFLAGS*>(data)->dwFlags = WSF_VISIBLE; return TRUE; }
#define TaskDialogIndirect FakeDialog
#define SendMessageW FakeMessage
#define GetUserObjectInformationW FakeUserObject
#include "../src/LauncherAutomation.cpp"
#undef TaskDialogIndirect
#undef SendMessageW
#undef GetUserObjectInformationW
#define CreateJobObjectW TestCreateJob
#define QueryInformationJobObject TestQueryJob
#define GetEnvironmentStringsW TestEnvironmentStrings
#define FreeEnvironmentStringsW TestFreeEnvironment
#include "../src/CompanionRunner.cpp"
#undef CreateJobObjectW
#undef QueryInformationJobObject
#undef GetEnvironmentStringsW
#undef FreeEnvironmentStringsW
using automation::Json;
using automation::Handle;
using automation::Invalid;
using chain::Join;
#define REQUIRE(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); return 20; } } while (0)
static std::wstring Env(PCWSTR name) { wchar_t value[32768]{}; GetEnvironmentVariableW(name, value, 32768); return value; }
static void Write(const std::wstring& path, const Json& value, bool replace = false) {
    Handle file(CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, replace ? CREATE_ALWAYS : CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file) throw automation::IoError(GetLastError()); automation::WriteBytes(file.value, automation::WriteJson(value));
}
static std::wstring Stamp(HANDLE process) {
    FILETIME birth{}, end{}, kernel{}, user{}; if (!GetProcessTimes(process, &birth, &end, &kernel, &user)) throw Invalid("times");
    wchar_t value[17]{}; swprintf_s(value, L"%08lx%08lx", birth.dwHighDateTime, birth.dwLowDateTime); return value;
}
static int Fixture(int argc, wchar_t** argv) {
    const auto c = automation::ParseJson(automation::ReadBytes(Env(L"CGL_CONTEXT_FILE")));
    const auto& config = c.At(L"input").At(L"config"); const auto mode = config.At(L"mode").String();
    const auto attempt = static_cast<unsigned>(c.At(L"entry").At(L"attempt").Integer(1, 3));
    auto captured = Json::Object(); captured[L"pid"] = Json::Number(GetCurrentProcessId()); captured[L"created"] = Json(Stamp(GetCurrentProcess()));
    captured[L"args"] = Json::Array(); for (int i = 1; i < argc; ++i) captured[L"args"].array.emplace_back(argv[i]);
    captured[L"input"] = c.At(L"input"); captured[L"guard"] = Json(Env(L"CHATGPT_PROXY_LAUNCHER_COMPANION"));
    captured[L"protocol"] = Json(Env(L"CGL_CHAIN_PROTOCOL")); captured[L"runId"] = c.At(L"runId"); captured[L"token"] = c.At(L"token");
    wchar_t directory[32768]{}; GetCurrentDirectoryW(32768, directory); captured[L"directory"] = Json(directory);
    if (mode == L"child-alive" || mode == L"task-child") {
        std::wstring command = automation::Quote(argv[0]) + L" --linger"; STARTUPINFOW si{sizeof(si)}; PROCESS_INFORMATION pi{};
        if (!CreateProcessW(argv[0], command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) return 90;
        Handle child(pi.hProcess), thread(pi.hThread); captured[L"childPid"] = Json::Number(pi.dwProcessId); captured[L"childCreated"] = Json(Stamp(child.value));
    }
    Write(config.At(L"capture").String() + L"-" + std::to_wstring(attempt) + L".json", captured);
    if (mode == L"task" || mode == L"task-child") return 0;
    if (mode == L"idle30") { Sleep(31000); return 0; }
    if (mode == L"timeout") { Sleep(1800); return 0; }
    if (mode == L"late") Sleep(1500);
    auto reply = Json::Object(); reply[L"schemaVersion"] = Json::Number(1); reply[L"runId"] = c.At(L"runId");
    reply[L"attemptId"] = c.At(L"attemptId"); reply[L"token"] = c.At(L"token"); reply[L"status"] = Json(L"ready");
    if ((mode == L"retry" && attempt == 1) || mode == L"permanent" || mode == L"child-alive") {
        reply[L"status"] = Json(L"failed"); reply[L"errorCode"] = Json(L"fixture-failure"); reply[L"retryable"] = Json(mode != L"permanent");
    }
    if (mode == L"wrong-token") reply[L"token"] = Json(L"incorrect");
    if (mode == L"invalid-failure") {
        reply[L"status"] = Json(L"failed"); reply[L"errorCode"] = Json(L"must-not-leak");
        reply[L"retryable"] = Json(true); reply[L"message"] = Json::Number(1);
    }
    if (mode == L"already-running") {
        const DWORD pid = static_cast<DWORD>(config.At(L"existingPid").Integer(1, MAXDWORD)); Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
        if (!process) return 91; reply[L"status"] = Json(L"already-running"); reply[L"readyProcessId"] = Json::Number(pid); reply[L"readyProcessCreationTime"] = Json(Stamp(process.value));
    }
    const auto path = c.At(L"replyPath").String(); Write(path + L".tmp", reply);
    if (!MoveFileExW((path + L".tmp").c_str(), path.c_str(), MOVEFILE_WRITE_THROUGH)) return 92;
    if (mode == L"invalid-failure") { Sleep(1800); return 0; }
    if (reply.At(L"status").String() == L"failed" || mode == L"crash" || mode == L"already-running") return mode == L"crash" ? 4 : 0;
    Sleep(1300); return 0;
}
static DWORD Legacy(PCWSTR, PCWSTR, PCWSTR, int) { ++legacyCalls; return ERROR_SUCCESS; }
static setup::StartupReport Disable() { throw Invalid("unexpected-system-action"); }
struct Case {
    std::wstring root, package, manifest;
    setup::Paths paths;
    unsigned results = 0;
    Json document;
    Case(const std::wstring& base, PCWSTR name, const std::wstring& self, const std::wstring& mode, bool safe = false, DWORD attempts = 1) {
        root = Join(base, name); if (!CreateDirectoryW(root.c_str(), nullptr)) throw automation::IoError(GetLastError());
        package = Join(root, L"package"); if (!CreateDirectoryW(package.c_str(), nullptr)) throw automation::IoError(GetLastError());
        if (!CopyFileW(self.c_str(), Join(package, L"Addon.exe").c_str(), TRUE)) throw automation::IoError(GetLastError());
        paths = {Join(root, L"state"), Join(root, L"startup.lnk"), Join(Join(root, L"state"), L"Launcher settings.lnk"), Join(root, L"desktop.lnk"), Join(root, L".env")};
        manifest = Join(package, L"companion.json");
        document = automation::ParseJson("{\"schemaVersion\":1,\"id\":\"org.example.addon\",\"name\":\"Fixture\",\"version\":\"1.0.0\",\"requires\":[\"ready-file-v1\"],\"entry\":{\"runtime\":\"exe\",\"path\":\"Addon.exe\",\"arguments\":[\"--fixture\"]},\"readiness\":{\"mode\":\"signal\",\"timeoutMs\":3000},\"retry\":{\"maxAttempts\":1,\"delayMs\":250,\"safe\":false},\"config\":{}}");
        document[L"config"][L"mode"] = Json(mode); document[L"config"][L"capture"] = Json(Join(root, L"capture"));
        document[L"entry"][L"window"] = Json(L"hidden");
        document[L"retry"][L"safe"] = Json(safe); document[L"retry"][L"maxAttempts"] = Json::Number(attempts);
        if (safe) document[L"requires"].array.emplace_back(L"tracked-retry-v1");
        if (mode == L"task" || mode == L"task-child") { document[L"readiness"][L"mode"] = Json(L"exit-code"); document[L"requires"] = Json::Array(); if (safe) document[L"requires"].array.emplace_back(L"tracked-retry-v1"); }
        Save();
    }
    void Save() { Write(manifest, document, true); }
    Json Call(const std::wstring& operation, Json request = Json::Object(), bool replace = false) {
        setup::AutomationCommand command; command.operation = operation; command.resultPath = Join(root, L"api-" + std::to_wstring(++results) + L".json");
        if (operation == L"companion-register") command.manifestPath = manifest;
        if (operation == L"companion-remove") command.entryId = document.At(L"id").String();
        command.replaceManifest = replace; command.request = request; return setup::RunAutomation(command, paths, {nullptr, 0}, Disable);
    }
    void Register() { auto r = Call(L"companion-register"); if (r.At(L"exitCode").Integer(0, 255)) throw Invalid(automation::Utf8(r.At(L"code").String()).c_str()); }
    Json Run() { chain::RunOptions o; o.root = paths.defaultDirectory; o.launcher = Join(root, L"ChatGPTProxyLauncher.exe"); o.feedback = 0; return chain::Run(o, L"C:\\isolated\\ChatGPT.exe", Legacy); }
    Json Capture(unsigned attempt = 1) { return automation::ParseJson(automation::ReadBytes(Join(root, L"capture-" + std::to_wstring(attempt) + L".json"))); }
};
static const Json& Last(const Json& report) { return report.At(L"entries").array.back(); }
static ULONGLONG CpuTime() {
    FILETIME born{}, end{}, kernel{}, user{}; GetProcessTimes(GetCurrentProcess(), &born, &end, &kernel, &user);
    return (static_cast<ULONGLONG>(kernel.dwHighDateTime) << 32) + kernel.dwLowDateTime + (static_cast<ULONGLONG>(user.dwHighDateTime) << 32) + user.dwLowDateTime;
}
static int Benchmark(const std::wstring& base, const std::wstring& self) {
    chain::RunOptions options; options.root = Join(base, L"empty"); options.feedback = 0;
    LARGE_INTEGER frequency{}; QueryPerformanceFrequency(&frequency);
    auto sample = [&](bool modern) {
        std::vector<double> times;
        for (unsigned i = 0; i < 220; ++i) {
            LARGE_INTEGER first{}, last{}; QueryPerformanceCounter(&first);
            if (modern) chain::Run(options, L"C:\\isolated\\ChatGPT.exe", Legacy);
            else companions::RunFolder(Join(options.root, companions::Folder), L"C:\\isolated\\ChatGPT.exe", Legacy, nullptr, MAXULONGLONG);
            QueryPerformanceCounter(&last);
            if (i >= 20) times.push_back((last.QuadPart - first.QuadPart) * 1000.0 / frequency.QuadPart);
        }
        std::sort(times.begin(), times.end()); return times;
    };
    const auto old = sample(false);
    PROCESS_MEMORY_COUNTERS_EX before{}, after{}; before.cb = sizeof(before); after.cb = sizeof(after);
    REQUIRE(GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&before), sizeof(before)));
    DWORD handlesBefore = 0, handlesAfter = 0; REQUIRE(GetProcessHandleCount(GetCurrentProcess(), &handlesBefore));
    const auto current = sample(true);
    REQUIRE(GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&after), sizeof(after)));
    REQUIRE(GetProcessHandleCount(GetCurrentProcess(), &handlesAfter) && handlesBefore == handlesAfter);
    printf("EMPTY 200 samples ms: legacy p50=%.4f p95=%.4f; chain p50=%.4f p95=%.4f; private delta=%lld; peak commit delta=%lld; handles delta=0\n",
        old[100], old[189], current[100], current[189], static_cast<long long>(after.PrivateUsage) - before.PrivateUsage,
        static_cast<long long>(after.PeakPagefileUsage) - before.PeakPagefileUsage);
    Case idle(base, L"idle30", self, L"idle30", true); idle.document[L"readiness"][L"timeoutMs"] = Json::Number(30000); idle.Save(); idle.Register();
    const auto cpu = CpuTime(), started = GetTickCount64(); const auto report = idle.Run();
    const auto elapsed = GetTickCount64() - started; const double spent = (CpuTime() - cpu) / 10000.0;
    REQUIRE(Last(report).At(L"status").String() == L"timeout-running");
    printf("WAIT elapsed=%llu ms; parent CPU=%.3f ms; child not killed\n", elapsed, spent);
    Handle child(OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(idle.Capture().At(L"pid").Integer(1, MAXDWORD))));
    REQUIRE(child && WaitForSingleObject(child.value, 3000) == WAIT_OBJECT_0);
    return 0;
}
int wmain(int argc, wchar_t** argv) {
    if (argc > 1 && !wcscmp(argv[1], L"--linger")) { Sleep(2200); return 0; }
    try {
        if (!Env(L"CGL_CONTEXT_FILE").empty()) return Fixture(argc, argv);
        const bool benchmark = argc == 3 && !wcscmp(argv[2], L"--benchmark");
        if (argc != 2 && !benchmark) return 2;
        REQUIRE(SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)));
        SetEnvironmentVariableW(L"CHATGPT_PROXY_LAUNCHER_COMPANION", nullptr);
        const auto base = Join(argv[1], L"chain-tests-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
        REQUIRE(CreateDirectoryW(base.c_str(), nullptr)); wchar_t self[32768]{}; GetModuleFileNameW(nullptr, self, 32768);
        if (benchmark) { const auto code = Benchmark(base, self); CoUninitialize(); return code; }
        {
            syntheticEnvironment = L"Alpha=old\0=C:=C:\\example\0Keep=value\0";
            const std::map<std::wstring, std::wstring> changes{{L"alpha", L"新 值"}, {L"Beta", L"a=b"}};
            const wchar_t expected[] = L"=C:=C:\\example\0Alpha=新 值\0Beta=a=b\0Keep=value\0";
            const auto block = chain::Environment(changes);
            REQUIRE(block == std::vector<wchar_t>(expected, expected + _countof(expected)));
            syntheticEnvironment = L"\0";
            REQUIRE(chain::Environment({}) == std::vector<wchar_t>({0, 0}));
            syntheticEnvironment = nullptr;
        }
        chain::RunOptions emptyOptions{Join(base, L"allocation-faults"), self, 0};
        REQUIRE(CreateDirectoryW(emptyOptions.root.c_str(), nullptr));
        REQUIRE(CreateDirectoryW(Join(emptyOptions.root, companions::Folder).c_str(), nullptr));
        chain::Run(emptyOptions, L"ChatGPT.exe", Legacy); // Warm up Windows security/CRT state before counting handles.
        DWORD handlesBeforeFaults = 0, handlesAfterFaults = 0;
        REQUIRE(GetProcessHandleCount(GetCurrentProcess(), &handlesBeforeFaults));
        unsigned failedReports = 0, completedReports = 0;
        for (int fail = 0; fail < 128; ++fail) {
            Json report;
            try { memory_fault::Scope fault(fail); report = chain::Run(emptyOptions, L"ChatGPT.exe", Legacy); }
            catch (const std::bad_alloc&) { ++failedReports; }
            if (report.type != Json::Type::Null) {
                ++completedReports;
                REQUIRE(report.At(L"exitCode").Integer(0, 255) == 0);
            }
            REQUIRE(chain::legacyResults == nullptr && chain::activeRecords == nullptr);
        }
        REQUIRE(failedReports > 0 && completedReports > 0);
        REQUIRE(GetProcessHandleCount(GetCurrentProcess(), &handlesAfterFaults));
        if (handlesBeforeFaults != handlesAfterFaults) printf("Fault-loop handles: %lu -> %lu\n", handlesBeforeFaults, handlesAfterFaults);
        REQUIRE(handlesBeforeFaults == handlesAfterFaults);
        {
            const std::wstring emptyTail;
            memory_fault::Scope fault(0);
            REQUIRE(setup::RunSetupCommandLine(nullptr, emptyTail) == 50);
        }
        const auto failedOutput = Join(base, L"allocation-result.json");
        {
            chain::Invocation invocation;
            REQUIRE(invocation.Parse((L"--launcher-chain --result " + automation::Quote(failedOutput) + L" --feedback quiet --").c_str()) == 0);
            const auto report = Json::Object();
            memory_fault::Scope fault(0);
            REQUIRE(invocation.Finish(0, report) == 70);
        }
        puts("PASS chain allocation failures: 128 positions, exceptions reach caller, no leaked handles or dangling callbacks; result failure returns 70");
        const chain::ReplyIdentity identity{L"run", L"attempt", L"token"};
        auto reply = automation::ParseJson("{\"schemaVersion\":1,\"runId\":\"run\",\"attemptId\":\"attempt\",\"token\":\"token\",\"status\":\"failed\",\"errorCode\":\"invalid-config\",\"retryable\":true}");
        REQUIRE(!chain::ParseReadyReply(automation::WriteJson(reply), identity).retryable);
        auto rejectedReply = [&](const Json& j) { try { chain::ParseReadyReply(automation::WriteJson(j), identity); return false; } catch (const Invalid&) { return true; } };
        reply[L"message"] = Json::Number(1); REQUIRE(rejectedReply(reply)); reply.object.erase(L"message");
        reply[L"token"] = Json(L"other"); REQUIRE(rejectedReply(reply)); reply[L"token"] = Json(L"token");
        reply[L"status"] = Json(L"ready"); REQUIRE(rejectedReply(reply)); // Status-specific fields must not leak between variants.
        Case repair(base, L"disable-broken", self, L"task"); repair.Register();
        Write(repair.manifest, Json::Object(), true);
        auto disableBroken = automation::ParseJson("{\"schemaVersion\":1,\"id\":\"org.example.addon\",\"overrides\":{\"enabled\":false}}");
        REQUIRE(repair.Call(L"companion-configure", disableBroken).At(L"exitCode").Integer(0, 255) == 0);
        REQUIRE(Last(repair.Run()).At(L"status").String() == L"disabled");
        disableBroken[L"overrides"][L"enabled"] = Json(true);
        REQUIRE(repair.Call(L"companion-configure", disableBroken).At(L"exitCode").Integer(0, 255) == 20);
        REQUIRE(Last(repair.Run()).At(L"status").String() == L"disabled");
        Case full(base, L"remove-over-limit", self, L"task"); full.Register();
        for (unsigned i = 0; i < 32; ++i) Write(Join(Join(full.paths.defaultDirectory, L"CompanionRegistry"), L"org.example.extra" + std::to_wstring(i) + L".json"), Json::Object());
        disableBroken[L"overrides"][L"enabled"] = Json(false);
        auto disabledFull = full.Call(L"companion-configure", disableBroken);
        REQUIRE(disabledFull.At(L"exitCode").Integer(0, 255) == 10 && disabledFull.At(L"committed").Bool());
        auto removed = full.Call(L"companion-remove");
        REQUIRE(removed.At(L"exitCode").Integer(0, 255) == 10 && removed.At(L"committed").Bool());
        REQUIRE(GetFileAttributesW(Join(Join(full.paths.defaultDirectory, L"CompanionRegistry"), L"org.example.addon.json").c_str()) == INVALID_FILE_ATTRIBUTES);
        Case ready(base, L"ready 中文", self, L"ready");
        ready.document[L"entry"][L"arguments"].array.emplace_back(L""); ready.document[L"entry"][L"arguments"].array.emplace_back(L"空格 quote\" slash\\ & % !"); ready.Save(); ready.Register();
        auto r = ready.Run(); REQUIRE(r.At(L"exitCode").Integer(0, 255) == 0 && Last(r).At(L"status").String() == L"ready");
        auto captured = ready.Capture(); REQUIRE(captured.At(L"args").array[1].String().empty() && captured.At(L"args").array[2].String() == L"空格 quote\" slash\\ & % !");
        REQUIRE(captured.At(L"guard").String() == L"1" && captured.At(L"protocol").String() == L"1" && captured.At(L"directory").String() == ready.package);
        Case retry(base, L"retry", self, L"retry", true, 2); retry.Register(); r = retry.Run();
        REQUIRE(Last(r).At(L"status").String() == L"ready" && Last(r).At(L"attempts").Integer(1, 3) == 2);
        REQUIRE(retry.Capture(1).At(L"token").String() != retry.Capture(2).At(L"token").String());
        Case permanent(base, L"permanent", self, L"permanent", true, 3); permanent.Register(); r = permanent.Run();
        REQUIRE(Last(r).At(L"status").String() == L"failed" && Last(r).At(L"attempts").Integer(1, 3) == 1);
        Case residual(base, L"child-alive", self, L"child-alive", true, 3); residual.document[L"readiness"][L"timeoutMs"] = Json::Number(1000); residual.Save(); residual.Register(); r = residual.Run();
        REQUIRE(Last(r).At(L"status").String() == L"timeout-running" && Last(r).At(L"attempts").Integer(1, 3) == 1);
        Case taskChild(base, L"task-child", self, L"task-child", true, 2); taskChild.document[L"readiness"][L"timeoutMs"] = Json::Number(1000); taskChild.Save(); taskChild.Register(); r = taskChild.Run();
        REQUIRE(Last(r).At(L"status").String() == L"timeout-running" && Last(r).At(L"attempts").Integer(1, 3) == 1);
        Case late(base, L"late", self, L"late", true, 2); late.document[L"readiness"][L"timeoutMs"] = Json::Number(1000); late.Save(); late.Register(); r = late.Run();
        REQUIRE(Last(r).At(L"status").String() == L"timeout-running" && Last(r).At(L"attempts").Integer(1, 3) == 1);
        Case timeout(base, L"timeout", self, L"timeout", true, 3); timeout.document[L"readiness"][L"timeoutMs"] = Json::Number(1000); timeout.Save(); timeout.Register(); r = timeout.Run();
        REQUIRE(Last(r).At(L"status").String() == L"timeout-running" && Last(r).At(L"attempts").Integer(1, 3) == 1);
        Case wrong(base, L"wrong-token", self, L"wrong-token"); wrong.Register(); r = wrong.Run(); REQUIRE(Last(r).At(L"status").String() == L"failed");
        Case invalidFailure(base, L"invalid-failure", self, L"invalid-failure", true, 2);
        invalidFailure.document[L"readiness"][L"timeoutMs"] = Json::Number(1000); invalidFailure.Save(); invalidFailure.Register(); r = invalidFailure.Run();
        REQUIRE(Last(r).At(L"status").String() == L"timeout-running" && Last(r).At(L"code").String() == L"ready-timeout" && Last(r).At(L"attempts").Integer(1, 3) == 1);
        Case crash(base, L"crash", self, L"crash", true, 2); crash.Register(); r = crash.Run();
        REQUIRE(Last(r).At(L"status").String() == L"failed" && Last(r).At(L"attempts").Integer(1, 3) == 2);
        Case existing(base, L"existing", self, L"already-running"); existing.document[L"config"][L"existingPid"] = Json::Number(GetCurrentProcessId()); existing.Save(); existing.Register(); r = existing.Run();
        REQUIRE(Last(r).At(L"status").String() == L"already-running");
        Case task(base, L"task", self, L"task", true); task.Register(); r = task.Run(); REQUIRE(Last(r).At(L"status").String() == L"completed");
        Case noTracking(base, L"tracking-unavailable", self, L"task", true, 3); noTracking.Register();
        rejectJob = true; r = noTracking.Run(); rejectJob = false;
        REQUIRE(Last(r).At(L"code").String() == L"tracking-unavailable" && Last(r).At(L"attempts").Integer(1, 3) == 1);
        REQUIRE(GetFileAttributesW(Join(noTracking.root, L"capture-1.json").c_str()) == INVALID_FILE_ATTRIBUTES);
        Case unknownTracking(base, L"tracking-unknown", self, L"task", true, 3); unknownTracking.Register();
        rejectJobQuery = true; r = unknownTracking.Run(); rejectJobQuery = false;
        REQUIRE(Last(r).At(L"status").String() == L"tracking-unknown" && Last(r).At(L"attempts").Integer(1, 3) == 1);
        auto config = automation::ParseJson("{\"schemaVersion\":1,\"id\":\"org.example.addon\",\"overrides\":{\"enabled\":false,\"arguments\":[\"kept\"]}}");
        REQUIRE(task.Call(L"companion-configure", config).At(L"exitCode").Integer(0, 255) == 0);
        task.document[L"version"] = Json(L"1.1.0"); task.Save(); task.Register();
        auto entries = chain::ReadRegistry(task.paths.defaultDirectory); REQUIRE(entries.size() == 1 && !entries[0].entry.enabled && entries[0].entry.arguments[0] == L"kept");
        REQUIRE(Last(task.Run()).At(L"status").String() == L"disabled");
        const auto old = task.manifest; task.manifest = Join(task.package, L"moved.json"); task.Save();
        REQUIRE(task.Call(L"companion-register").At(L"exitCode").Integer(0, 255) == 30);
        REQUIRE(task.Call(L"companion-register", Json::Object(), true).At(L"exitCode").Integer(0, 255) == 0);
        REQUIRE(task.Call(L"companion-remove").At(L"exitCode").Integer(0, 255) == 0 && chain::ReadRegistry(task.paths.defaultDirectory).empty());
        REQUIRE(GetFileAttributesW(old.c_str()) != INVALID_FILE_ATTRIBUTES && GetFileAttributesW(Join(task.package, L"Addon.exe").c_str()) != INVALID_FILE_ATTRIBUTES);
        Case unsafe(base, L"unsafe", self, L"task", false, 2); REQUIRE(unsafe.Call(L"companion-register").At(L"exitCode").Integer(0, 255) == 20);
        unsafe.document[L"retry"][L"maxAttempts"] = Json::Number(1); unsafe.document[L"entry"][L"path"] = Json(L"..\\outside.exe"); unsafe.Save();
        REQUIRE(unsafe.Call(L"companion-register").At(L"exitCode").Integer(0, 255) == 20);
        Case missingDep(base, L"missing-dependency", self, L"task");
        missingDep.document[L"dependsOn"] = automation::ParseJson("[{\"id\":\"org.example.missing\"}]"); missingDep.Save();
        REQUIRE(missingDep.Call(L"companion-register").At(L"exitCode").Integer(0, 255) == 20);
        Case batch(base, L"batch", self, L"task");
        const auto script = Join(batch.package, L"tool & %CGL_TEST_LITERAL%!.cmd");
        { Handle f(CreateFileW(script.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr)); REQUIRE(f); automation::WriteBytes(f.value, "@echo off\r\nexit /b 0\r\n"); }
        batch.document[L"entry"][L"runtime"] = Json(L"batch"); batch.document[L"entry"][L"path"] = Json(L"tool & %CGL_TEST_LITERAL%!.cmd");
        const auto injected = Join(batch.root, L"injected.txt");
        batch.document[L"entry"][L"arguments"] = Json::Array(); batch.document[L"entry"][L"arguments"].array.emplace_back(L"& echo forbidden > " + automation::Quote(injected));
        batch.Save(); batch.Register(); r = batch.Run();
        REQUIRE(Last(r).At(L"status").String() == L"completed" && GetFileAttributesW(injected.c_str()) == INVALID_FILE_ATTRIBUTES);
        Case budget(base, L"budget", self, L"timeout"); budget.document[L"id"] = Json(L"org.example.a"); budget.Save(); budget.Register();
        budget.manifest = Join(budget.package, L"second.json"); budget.document[L"id"] = Json(L"org.example.b"); budget.Save(); budget.Register();
        Write(Join(budget.paths.defaultDirectory, L"companion-policy.json"), automation::ParseJson("{\"schemaVersion\":1,\"budgetMs\":1000,\"feedback\":\"quiet\"}"));
        r = budget.Run(); REQUIRE(r.At(L"entries").array.size() == 2 && Last(r).At(L"status").String() == L"skipped-budget");
        Case graph(base, L"graph", self, L"task"); graph.document[L"id"] = Json(L"org.example.a"); graph.Save(); graph.Register();
        const auto aFile = graph.manifest; auto aDoc = graph.document;
        graph.manifest = Join(graph.package, L"b.json"); graph.document[L"id"] = Json(L"org.example.b"); graph.document[L"config"][L"capture"] = Json(Join(graph.root, L"capture-b"));
        graph.document[L"dependsOn"] = automation::ParseJson("[{\"id\":\"org.example.a\",\"minimumVersion\":\"1.0.0\"}]"); graph.Save(); graph.Register();
        r = graph.Run(); REQUIRE(r.At(L"entries").array.size() == 2 && r.At(L"entries").array[0].At(L"id").String() == L"org.example.a" && Last(r).At(L"status").String() == L"completed");
        aDoc[L"dependsOn"] = automation::ParseJson("[{\"id\":\"org.example.b\"}]"); Write(aFile, aDoc, true);
        r = graph.Run(); REQUIRE(r.At(L"exitCode").Integer(0, 255) == 10 && Last(r).At(L"code").String() == L"invalid-graph");
        aDoc.object.erase(L"dependsOn"); Write(aFile, aDoc, true);
        config[L"id"] = Json(L"org.example.a");
        REQUIRE(graph.Call(L"companion-configure", config).At(L"exitCode").Integer(0, 255) == 10);
        r = graph.Run(); REQUIRE(Last(r).At(L"status").String() == L"skipped-dependency");
        config[L"id"] = Json(L"org.example.b");
        Write(Join(Join(graph.paths.defaultDirectory, L"CompanionRegistry"), L"org.example.b.json"), Json::Object(), true);
        REQUIRE(graph.Call(L"companion-register").At(L"exitCode").Integer(0, 255) == 30);
        const auto foreignRemoval = graph.Call(L"companion-remove");
        REQUIRE(foreignRemoval.At(L"exitCode").Integer(0, 255) == 30 && !foreignRemoval.At(L"committed").Bool());
        REQUIRE(automation::ReadBytes(Join(Join(graph.paths.defaultDirectory, L"CompanionRegistry"), L"org.example.b.json")) == automation::WriteJson(Json::Object()));
        Case duplicate(base, L"legacy-duplicate", self, L"task"); duplicate.Register(); REQUIRE(setup::AddCompanionProgram(duplicate.paths, Join(duplicate.package, L"Addon.exe")));
        r = duplicate.Run(); REQUIRE(legacyCalls == 0 && r.At(L"entries").array[0].At(L"code").String() == L"legacy-entry-conflict" && Last(r).At(L"status").String() == L"completed");
        REQUIRE(duplicate.Call(L"companion-register").At(L"exitCode").Integer(0, 255) == 30);
        {
            automation::UserLock lock; REQUIRE(lock.Acquire(Join(duplicate.paths.defaultDirectory, L"chain-start")) == 0);
            Json busy; std::thread other([&] { busy = duplicate.Run(); }); other.join();
            REQUIRE(busy.At(L"chainStatus").String() == L"skipped-busy" && busy.At(L"entries").array.empty());
        }
        REQUIRE(notifications == 0); chain::NotifyFailures(r); REQUIRE(notifications == 1 && closeSignals == 1);
        REQUIRE(chain::DeploymentInfo(ready.paths.defaultDirectory).At(L"state").String() == L"not-deployed");
        const auto output = Join(base, L"chain-result.json");
        {
        chain::Invocation invocation;
        const std::wstring tail = L"--launcher-chain --result " + automation::Quote(output) + L" --feedback quiet --   --proxy-server=x  \"空 格\"  ";
        REQUIRE(invocation.Parse(tail.c_str()) == 0 && std::wstring(invocation.forwarded) == L"--proxy-server=x  \"空 格\"  ");
        REQUIRE(invocation.Finish(ERROR_FILE_NOT_FOUND, Json{}) == 50);
        }
        REQUIRE(automation::ParseJson(automation::ReadBytes(output)).At(L"chatgpt").At(L"processCreated").Bool() == false);
        puts("PASS chain: real isolated child readiness, fresh-token retry, permanent failures, residual child/no duplicate, timeouts, bad tokens, early exit, existing instance, task exit codes, registry overrides/migration/remove, graph cycles, legacy conflict and timed notification (UI mocked)");
        CoUninitialize(); return 0;
    } catch (const std::exception& e) { printf("FAIL exception: %s\n", e.what()); return 20; }
    catch (const automation::IoError& e) { printf("FAIL I/O: %lu\n", e.code); return 20; }
}
