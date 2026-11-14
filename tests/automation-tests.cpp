// SPDX-License-Identifier: GPL-3.0-only
#include "../src/SetupCommandLine.h"
#include "../src/DeploymentDiscovery.h"
#include <objbase.h>
#include <stdio.h>
#include <thread>
#include <atomic>
#define REQUIRE(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); return 20; } } while (0)
using namespace automation;
using namespace setup;
static unsigned startupCalls = 0;
static StartupReport Disable() { ++startupCalls; StartupReport r; r.packagedTask = true; r.packagedTasks = 1; r.packagedDisabled = 1; return r; }
static StartupReport DisableThrows() { throw Invalid("simulated-post-action-error"); }
static bool Rejected(const std::string& text) { try { ParseJson(text); return false; } catch (const Invalid&) { return true; } }
static void Write(const std::wstring& path, const std::string& bytes, bool replace = false) {
    Handle f(CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, replace ? CREATE_ALWAYS : CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!f) throw IoError(GetLastError()); WriteBytes(f.value, bytes);
}
int wmain(int argc, wchar_t** argv) {
    if (argc != 4) return 2;
    REQUIRE(SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)));
    try {
        REQUIRE(Rejected("{\"x\":1,\"x\":2}"));
        REQUIRE(Rejected("{\"x\":1,\"\\u0078\":2}"));
        REQUIRE(Rejected("{\"x\":\"\\uD800\"}"));
        REQUIRE(Rejected("{\"x\":\"\\u0000\"}"));
        REQUIRE(Rejected("{\"x\":1,}")); REQUIRE(Rejected("[1,]")); REQUIRE(Rejected("01")); REQUIRE(Rejected("1e"));
        REQUIRE(Rejected(std::string("\xff\xff", 2)));
        REQUIRE(Rejected(std::string(30, '[') + "0" + std::string(30, ']')));
        REQUIRE(WriteJson(ParseJson("{\"z\":[true,null,\"\\u4e2d\\u6587\",\"\\ud83d\\ude00\"]}")) == "{\"z\":[true,null,\"中文\",\"😀\"]}\n");
        REQUIRE(ParseJson("1.0").Integer(1, 1) == 1);
        REQUIRE(ParseJson("1.23e2").Integer(0, 255) == 123);
        bool fractionalRejected = false;
        try { ParseJson("1.00000000000000001").Integer(1, 1); } catch (const Invalid&) { fractionalRejected = true; }
        REQUIRE(fractionalRejected);
        for (const auto path : {L"C:\\COM\u00b9.json", L"C:\\safe\\LPT\u00b2.env", L"C:\\COM\u00b3"}) {
            bool rejected = false; try { FullPath(path); } catch (const Invalid&) { rejected = true; } REQUIRE(rejected);
        }
        REQUIRE(FullPath(L"C:\\COM10.json") == L"C:\\COM10.json");
        const auto root = Join(argv[1], L"automation-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
        REQUIRE(CreateDirectoryW(root.c_str(), nullptr));
        Paths p{Join(root, L"state"), Join(root, L"Startup.lnk"), Join(Join(root, L"state"), L"settings.lnk"), Join(root, L"Desktop.lnk"), Join(root, L".env")};
        const auto bytes = ReadBytes(argv[2], 16 * 1024 * 1024);
        const Payload payload{reinterpret_cast<const BYTE*>(bytes.data()), bytes.size()};
        unsigned resultNumber = 0;
        auto Command = [&](const std::wstring& operation, Json request) {
            AutomationCommand c; c.operation = operation; c.request = request;
            c.resultPath = Join(root, L"result-" + std::to_wstring(++resultNumber) + L".json"); return c;
        };
        auto request = ParseJson("{\"schemaVersion\":1,\"settings\":{\"extraArguments\":[\"\",\"中文 a\",\"quote\\\"end\",\"& % !\"]}}");
        auto plan = Command(L"plan", request); auto result = RunAutomation(plan, p, payload, Disable);
        REQUIRE(result.At(L"exitCode").Integer(0, 255) == 0);
        REQUIRE(GetFileAttributesW(p.defaultDirectory.c_str()) == INVALID_FILE_ATTRIBUTES);
        REQUIRE(!result.At(L"data").At(L"startup").Bool() && !result.At(L"data").At(L"writesCodexEnv").Bool());
        REQUIRE(startupCalls == 0);
        auto deploy = Command(L"deploy", request); result = RunAutomation(deploy, p, payload, Disable);
        REQUIRE(result.At(L"exitCode").Integer(0, 255) == 0 && result.At(L"committed").Bool());
        Loaded loaded; REQUIRE(Load(p, loaded) && loaded.deployed && !loaded.options.startup && !loaded.options.silent);
        Link link; REQUIRE(ReadLink(p.settingsLink, link));
        REQUIRE(link.arguments == ParseJson(ReadBytes(plan.resultPath)).At(L"data").At(L"arguments").String());
        auto info = Command(L"info", Json::Object()); result = RunAutomation(info, p, payload, Disable);
        REQUIRE(result.At(L"data").At(L"state").String() == L"managed");
        const auto discoveryPath = Join(p.defaultDirectory, contract::DiscoveryFile);
        const auto validDiscovery = ReadBytes(discoveryPath);
        for (unsigned test = 0; test < 4; ++test) {
            auto bad = ParseJson(validDiscovery);
            if (test == 0) bad[L"capabilities"] = Json(L"not-an-array");
            if (test == 1) bad[L"setupApiVersions"].array.push_back(Json::Number(1));
            if (test == 2) bad[L"launcherVersion"] = Json(L"");
            if (test == 3) bad[L"unexpected"] = Json(true);
            Write(discoveryPath, WriteJson(bad), true);
            const auto queried = RunAutomation(Command(L"info", Json::Object()), p, payload, Disable);
            REQUIRE(queried.At(L"data").At(L"state").String() == L"invalid");
            REQUIRE(WriteJson(queried.At(L"data")) == WriteJson(ReadDeploymentInfo(p.defaultDirectory, p.settingsLink)));
        }
        Write(discoveryPath, validDiscovery, true);
        {
            Handle locked(CreateFileW(discoveryPath.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
            REQUIRE(locked);
            REQUIRE(ReadDeploymentInfo(p.defaultDirectory, p.settingsLink).At(L"state").String() == L"unknown");
        }
        {
            Handle locked(CreateFileW(p.settingsLink.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
            REQUIRE(locked);
            REQUIRE(ReadDeploymentInfo(p.defaultDirectory, p.settingsLink).At(L"state").String() == L"unknown");
        }
        // Discovery must not parse private launch arguments or unrelated startup files.
        Write(p.startupLink, "unrelated startup file");
        REQUIRE(RunAutomation(Command(L"info", Json::Object()), p, payload, Disable).At(L"data").At(L"state").String() == L"managed");
        REQUIRE(DeleteFileW(p.startupLink.c_str()));
        const auto fakeExe = Join(p.defaultDirectory, LauncherName); Write(fakeExe, "not a versioned application", true);
        Write(discoveryPath, WriteJson(MakeDiscoveryRecord(fakeExe, L"")), true);
        REQUIRE(ReadDeploymentInfo(p.defaultDirectory, p.settingsLink).At(L"state").String() == L"invalid");
        REQUIRE(DeleteFileW(fakeExe.c_str()));
        REQUIRE(ReadDeploymentInfo(p.defaultDirectory, p.settingsLink).At(L"state").String() == L"missing");
        Write(fakeExe, bytes); Write(discoveryPath, validDiscovery, true);
        const auto previous = ReadBytes(deploy.resultPath);
        result = RunAutomation(deploy, p, payload, Disable);
        REQUIRE(result.At(L"exitCode").Integer(0, 255) == 70 && ReadBytes(deploy.resultPath) == previous);
        Write(p.codexEnvFile, "KEEP=value\nALL_PROXY=http://old:1\n");
        request = ParseJson("{\"schemaVersion\":1,\"settings\":{\"proxy\":{\"enabled\":true,\"protocol\":\"socks5\",\"host\":\"::1\",\"port\":12345}},\"actions\":{\"codexEnv\":\"update\",\"officialStartup\":\"disable\"}}");
        auto env = Command(L"deploy", request); result = RunAutomation(env, p, payload, Disable);
        REQUIRE(result.At(L"exitCode").Integer(0, 255) == 10 && result.At(L"committed").Bool());
        REQUIRE(startupCalls == 1);
        const auto envBytes = ReadBytes(p.codexEnvFile);
        REQUIRE(envBytes.find("KEEP=value\n") != envBytes.npos && envBytes.find("socks5://[::1]:12345") != envBytes.npos);
        result = RunAutomation(Command(L"deploy", request), p, payload, DisableThrows);
        REQUIRE(result.At(L"exitCode").Integer(0, 255) == 10 && result.At(L"committed").Bool());
        auto leave = ParseJson("{\"schemaVersion\":1}");
        result = RunAutomation(Command(L"deploy", leave), p, payload, Disable);
        REQUIRE(result.At(L"exitCode").Integer(0, 255) == 0 && ReadBytes(p.codexEnvFile) == envBytes && startupCalls == 1);
        request[L"settings"][L"proxy"][L"port"] = Json::Number(0);
        result = RunAutomation(Command(L"deploy", request), p, payload, Disable);
        REQUIRE(result.At(L"exitCode").Integer(0, 255) == 20 && ReadBytes(p.codexEnvFile) == envBytes);
        request = ParseJson("{\"schemaVersion\":2}"); result = RunAutomation(Command(L"plan", request), p, payload, Disable);
        REQUIRE(result.At(L"exitCode").Integer(0, 255) == 21);
        request = ParseJson("{\"schemaVersion\":1,\"settings\":{\"proxy\":{\"enabled\":false}},\"actions\":{\"codexEnv\":\"update\"}}");
        result = RunAutomation(Command(L"deploy", request), p, payload, Disable); REQUIRE(result.At(L"exitCode").Integer(0, 255) == 20);
        auto foreign = Join(root, L"foreign"); REQUIRE(CreateDirectoryW(foreign.c_str(), nullptr)); Write(Join(foreign, LauncherName), "foreign");
        request = ParseJson("{\"schemaVersion\":1,\"settings\":{}}"); request[L"settings"][L"directory"] = Json(foreign);
        result = RunAutomation(Command(L"plan", request), p, payload, Disable);
        REQUIRE(result.At(L"exitCode").Integer(0, 255) == 30 && ReadBytes(Join(foreign, LauncherName)) == "foreign");
        auto protectedResult = Command(L"deploy", leave); protectedResult.resultPath = Join(p.defaultDirectory, L"discovery.json");
        const auto discovery = ReadBytes(protectedResult.resultPath);
        result = RunAutomation(protectedResult, p, payload, Disable);
        REQUIRE(result.At(L"exitCode").Integer(0, 255) == 70 && ReadBytes(protectedResult.resultPath) == discovery);
        protectedResult.resultPath = Join(p.defaultDirectory, L"companion-policy.json");
        result = RunAutomation(protectedResult, p, payload, Disable);
        REQUIRE(result.At(L"exitCode").Integer(0, 255) == 70 && GetFileAttributesW(protectedResult.resultPath.c_str()) == INVALID_FILE_ATTRIBUTES);
        AutomationCommand parsed;
        ParseAutomationCommand(L"--headless plan --result " + QuoteArgument(Join(root, L"flags.json")) + L" --proxy http://127.0.0.1:7897 --extra-arg \"\" --extra-arg --test", parsed);
        REQUIRE(parsed.request.At(L"settings").At(L"extraArguments").array[0].String().empty());
        REQUIRE(parsed.request.At(L"settings").At(L"extraArguments").array[1].String() == L"--test");
        auto badFlags = [&](const wchar_t* tail) { try { ParseAutomationCommand(tail, parsed); return false; } catch (const Invalid&) { return true; } };
        REQUIRE(badFlags(L"--headless plan --result C:\\test.json --startup on --startup off"));
        REQUIRE(badFlags(L"--headless plan --result C:\\test.json --request C:\\in.json --base defaults"));
        REQUIRE(badFlags(L"--headless plan --result C:\\test.json --clear-extra --extra-arg x"));
        REQUIRE(badFlags(L"--headless plan --result C:\\test.json --extra-arg"));
        {
            UserLock lock; REQUIRE(lock.Acquire(p.settingsLink) == 0);
            DWORD childResult = 0; std::thread other([&] { UserLock second; childResult = second.Acquire(p.settingsLink, 25); }); other.join();
            REQUIRE(childResult == ERROR_BUSY);
        }
        // Real EXE error path: it must exit without creating a GUI or running a deployment.
        const auto childResultPath = Join(root, L"native-error.json");
        std::wstring cmd = QuoteArgument(argv[3]) + L" --headless plan --result " + QuoteArgument(childResultPath) + L" --request " + QuoteArgument(Join(root, L"missing.json"));
        STARTUPINFOW si{sizeof(si)}; PROCESS_INFORMATION pi{};
        REQUIRE(CreateProcessW(argv[3], cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, root.c_str(), &si, &pi));
        Handle process(pi.hProcess), thread(pi.hThread);
        const auto waited = WaitForSingleObject(process.value, 5000);
        if (waited != WAIT_OBJECT_0) TerminateProcess(process.value, 99); // Only this isolated test child.
        REQUIRE(waited == WAIT_OBJECT_0); DWORD exitCode = 0; REQUIRE(GetExitCodeProcess(process.value, &exitCode) && exitCode != 0);
        REQUIRE(ParseJson(ReadBytes(childResultPath)).At(L"committed").Bool() == false);
        puts("PASS automation JSON, planning, isolated deployment, discovery, env merge, conflicts, argument boundaries, locking and native error exit");
    } catch (const std::exception& e) { printf("FAIL exception: %s\n", e.what()); return 20; }
    catch (const IoError& e) { printf("FAIL I/O: %lu\n", e.code); return 20; }
    CoUninitialize(); return 0;
}
