// SPDX-License-Identifier: GPL-3.0-only
#include "MemoryFault.h"
#include "SetupBridgeFixture.h"
#include "../src/SetupCommandLine.h"
#include "../src/SetupBridge.h"
#include <shlobj.h>
#include <thread>
#include <cstdio>

static int syntheticCommit = -1;
static bool failAfterCommit = false;
namespace setup {
Result BridgeTestApply(const Paths& paths, const Options& options, Payload payload, std::wstring& target) {
    if (syntheticCommit >= 0) {
        Result result{ERROR_WRITE_FAULT, L"Synthetic write failure"};
        result.commit = static_cast<CommitState>(syntheticCommit);
        return result;
    }
    auto result = Apply(paths, options, payload, target);
    if (result && failAfterCommit) { memory_fault::armed = true; memory_fault::remaining = 0; }
    return result;
}
}
#define Apply BridgeTestApply
#include "../src/SetupBridge.cpp"
#undef Apply

#define REQUIRE(x) do { if (!(x)) { printf("FAIL actions line %d: %s\n", __LINE__, #x); return 20; } } while (0)
int wmain(int argc, wchar_t** argv) {
    if (argc != 3) return 2;
    try {
        l10n::Select(L"zh-CN");
        const auto root = setup::Join(argv[1], L"bridge-actions-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
        REQUIRE(CreateDirectoryW(root.c_str(), nullptr));
        auto paths = BridgeTestPaths(root);
        const auto pathJson = BridgeTestPathJson(paths);
        setup::Options options; options.directory = paths.defaultDirectory; options.proxy = true; options.port = L"7890";
        options.extra = L"\"\" \"中文 参数\" \"quote\\\"end\"";
        Json reply;
        uint32_t commit = 0;
        auto call = [&](uint32_t action, const Json& request) {
            const auto input = automation::WriteJson(request);
            SetupBridgeBuffer buffer{};
            const auto code = setup_bridge_action(action, reinterpret_cast<const uint8_t*>(input.data()), input.size(),
                reinterpret_cast<const uint8_t*>(pathJson.data()), pathJson.size(), 0, &buffer, &commit);
            memory_fault::armed = false;
            reply = buffer.data ? automation::ParseJson(std::string(reinterpret_cast<char*>(buffer.data), buffer.length)) : Json{};
            memory_fault::watchedAllocation = buffer.data; memory_fault::watchedReleased = false;
            const bool hadBuffer = buffer.data != nullptr;
            setup_bridge_free(&buffer);
            if (hadBuffer && !memory_fault::watchedReleased) throw automation::Invalid("response-leak");
            return code;
        };
        REQUIRE(call(SETUP_VALIDATE, Options(options)) == 0 && reply.At(L"valid").Bool());
        REQUIRE(reply.At(L"arguments").String().find(options.extra) != std::wstring::npos);
        for (const auto& bad : {L"\"unclosed", L"bad\nline"}) {
            auto invalid = options; invalid.extra = bad;
            REQUIRE(call(SETUP_VALIDATE, Options(invalid)) == 0 && !reply.At(L"valid").Bool());
        }
        auto invalid = options; invalid.extra.assign(setup::MaxArguments + 1, L'x');
        REQUIRE(call(SETUP_VALIDATE, Options(invalid)) == 0 && !reply.At(L"valid").Bool());
        invalid = options; invalid.port = L"65536";
        REQUIRE(call(SETUP_VALIDATE, Options(invalid)) == 0 && !reply.At(L"valid").Bool());
        auto malformed = Options(options); malformed[L"startup"] = Json(L"true");
        REQUIRE(call(SETUP_SAVE, malformed) != 0 && commit == 0);
        malformed = Options(options); malformed[L"extraKey"] = Json(true);
        REQUIRE(call(SETUP_SAVE, malformed) != 0 && commit == 0);
        REQUIRE(GetFileAttributesW(paths.defaultDirectory.c_str()) == INVALID_FILE_ATTRIBUTES);
        REQUIRE(call(SETUP_SAVE, Options(options)) == 0 && commit == 1);
        REQUIRE(reply.At(L"commit").String() == L"saved");
        REQUIRE(reply.At(L"tasks").array.size() == 1 && reply.At(L"tasks").array[0].String() == L"restart");
        auto env = automation::ReadBytes(paths.codexEnvFile);
        REQUIRE(env.find("7890") != std::string::npos);
        const auto envWithUnmanaged = std::string("KEEP_ME=yes\r\n") + env;
        { HANDLE file = CreateFileW(paths.codexEnvFile.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
          REQUIRE(file != INVALID_HANDLE_VALUE); automation::Handle owned(file); automation::WriteBytes(file, envWithUnmanaged); }
        options.port = L"7891"; failAfterCommit = true;
        REQUIRE(call(SETUP_SAVE, Options(options)) == ERROR_NOT_ENOUGH_MEMORY && commit == 1);
        REQUIRE(reply.type == Json::Type::Null);
        REQUIRE(automation::ReadBytes(paths.codexEnvFile).find("7891") != std::string::npos);
        REQUIRE(automation::ReadBytes(paths.codexEnvFile).find("KEEP_ME=yes") != std::string::npos);
        failAfterCommit = false;
        for (int value : {0, 2, 3}) {
            syntheticCommit = value;
            REQUIRE(call(SETUP_SAVE, Options(options)) == 0 && commit == static_cast<uint32_t>(value));
            REQUIRE(reply.At(L"commit").String() == (value == 0 ? L"not-started" : value == 2 ? L"rolled-back" : L"rollback-incomplete"));
        }
        syntheticCommit = -1;
        // Hold the same user lock on another thread. No write may enter while it is held.
        HANDLE entered = CreateEventW(nullptr, TRUE, FALSE, nullptr), release = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        REQUIRE(entered && release);
        std::thread holder([&] { automation::UserLock lock; lock.Acquire(paths.settingsLink); SetEvent(entered); WaitForSingleObject(release, 10000); });
        WaitForSingleObject(entered, 10000);
        const auto blocked = call(SETUP_SAVE, Options(options));
        SetEvent(release); holder.join(); CloseHandle(entered); CloseHandle(release);
        REQUIRE(blocked == ERROR_BUSY && commit == 0);
        // Native selection imports the original .lnk bytes, including quoted arguments.
        const auto target = setup::Join(root, L"tool.exe"), shortcut = setup::Join(root, L"工具.lnk");
        REQUIRE(CopyFileW(argv[2], target.c_str(), TRUE));
        REQUIRE(SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)));
        IShellLinkW* link = nullptr;
        REQUIRE(SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link))));
        link->SetPath(target.c_str()); link->SetArguments(options.extra.c_str()); link->SetWorkingDirectory(root.c_str());
        IPersistFile* persist = nullptr; REQUIRE(SUCCEEDED(link->QueryInterface(IID_PPV_ARGS(&persist))));
        REQUIRE(SUCCEEDED(persist->Save(shortcut.c_str(), TRUE))); persist->Release(); link->Release(); CoUninitialize();
        auto pick = Json::Object(); pick[L"testSelection"] = Json(shortcut);
        REQUIRE(call(SETUP_ADD_PROGRAM, pick) == 0 && commit == 1);
        REQUIRE(reply.At(L"entry").At(L"path").String() == shortcut);
        REQUIRE(reply.At(L"entry").At(L"arguments").String() == options.extra);
        REQUIRE(automation::ReadBytes(shortcut) == automation::ReadBytes(setup::Join(setup::Join(paths.defaultDirectory, companions::Folder), L"工具.lnk")));
        REQUIRE(call(SETUP_ADD_PROGRAM, pick) == 0); // Idempotent import.
        pick[L"testSelection"] = Json(argv[2]);
        REQUIRE(call(SETUP_ADD_PROGRAM, pick) != 0 && commit == 0); // Outside harness root.
        pick[L"testSelection"] = Json(L"");
        REQUIRE(call(SETUP_DIRECTORY, pick) == 0 && reply.At(L"path").String().empty());
        auto prefs = Json::Object(); prefs[L"dark"] = Json(false); prefs[L"language"] = Json(L"ja-JP");
        REQUIRE(call(SETUP_PREFERENCES, prefs) == 0 && reply.At(L"preferences").At(L"resolvedLanguage").String() == L"ja-JP");
        prefs[L"language"] = Json(L"bad"); REQUIRE(call(SETUP_PREFERENCES, prefs) != 0);
        auto location = Json::Object(); location[L"location"] = Json(L"windows");
        REQUIRE(call(SETUP_OPEN_LOCATION, location) == 0); // No real Settings process.
        location[L"location"] = Json(L"cmd.exe"); REQUIRE(call(SETUP_OPEN_LOCATION, location) != 0);
        puts("PASS bridge actions: native validation, commit states, committed response OOM, isolated save/env preservation, user lock, original shortcut import, cancellation, preferences and fixed locations");
        return 0;
    } catch (...) { memory_fault::armed = false; puts("FAIL bridge action exception"); return 50; }
}
