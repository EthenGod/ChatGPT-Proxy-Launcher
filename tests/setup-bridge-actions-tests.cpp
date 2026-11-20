// SPDX-License-Identifier: GPL-3.0-only
#include "MemoryFault.h"
#include "SetupBridgeFixture.h"
#include "../src/SetupCommandLine.h"
#include "../src/SetupBridge.h"
#include "../src/SetupProgramIcons.h"
#include <shlobj.h>
#include <wrl/client.h>
#include <thread>
#include <cstdio>

static int syntheticCommit = -1;
static bool failAfterCommit = false;
static HANDLE saveEntered = nullptr, continueSave = nullptr;
static std::wstring unprobedTarget;
static setup::ui::ProgramDrive simulatedDrive = setup::ui::ProgramDrive::Unverified;
static unsigned targetProbes = 0;
namespace setup::ui {
ProgramDrive TestProgramDrive(const std::wstring& path) {
    return path == unprobedTarget ? simulatedDrive : ClassifyProgramDrive(path);
}
}
static DWORD WINAPI ObservedAttributes(PCWSTR path) {
    if (!unprobedTarget.empty() && unprobedTarget == path) ++targetProbes;
    return GetFileAttributesW(path);
}
namespace setup {
Result UnexpectedSystemPaths(Paths&) { return {ERROR_PATH_NOT_FOUND, L"System folders unavailable"}; }
Result BridgeTestApply(const Paths& paths, const Options& options, Payload payload, std::wstring& target, bool isolated) {
    if (saveEntered) {
        SetEvent(saveEntered);
        if (WaitForSingleObject(continueSave, 30000) != WAIT_OBJECT_0) return {ERROR_TIMEOUT, L"Test save timed out"};
    }
    if (syntheticCommit >= 0) {
        Result result{ERROR_WRITE_FAULT, L"Synthetic write failure"};
        result.commit = static_cast<CommitState>(syntheticCommit);
        return result;
    }
    auto result = Apply(paths, options, payload, target, isolated);
    if (result && failAfterCommit) { memory_fault::armed = true; memory_fault::remaining = 0; }
    return result;
}
}
#define Apply BridgeTestApply
#define GetPaths UnexpectedSystemPaths
#define ClassifyProgramDrive TestProgramDrive
#define GetFileAttributesW ObservedAttributes
#include "../src/SetupBridge.cpp"
#undef GetFileAttributesW
#undef ClassifyProgramDrive
#undef GetPaths
#undef Apply

#define REQUIRE(x) do { if (!(x)) { printf("FAIL actions line %d: %s\n", __LINE__, #x); return 20; } } while (0)
static void Put(const std::wstring& path, const std::string& bytes) {
    automation::Handle file(CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file) throw automation::IoError(GetLastError());
    automation::WriteBytes(file.value, bytes);
}
static void PutTestIcon(const std::wstring& path) {
    BITMAPINFOHEADER dib{};
    dib.biSize = sizeof(dib); dib.biWidth = 16; dib.biHeight = 32; dib.biPlanes = 1; dib.biBitCount = 32;
    constexpr DWORD maskBytes = 16 * 4;
    const DWORD entry[] = {sizeof(dib) + 16 * 16 * 4 + maskBytes, 22};
    std::string bytes("\0\0\1\0\1\0\x10\x10\0\0\1\0\x20\0", 14);
    bytes.append(reinterpret_cast<const char*>(entry), sizeof(entry));
    bytes.append(reinterpret_cast<const char*>(&dib), sizeof(dib));
    for (int i = 0; i < 16 * 16; ++i) bytes.append("\x73\xA6\x21\xFF", 4);
    bytes.append(maskBytes, '\0');
    Put(path, bytes);
}
static bool SaveTestLink(const std::wstring& path, const std::wstring& target, const std::wstring& icon,
    const std::wstring& arguments = {}, const std::wstring& directory = {}) {
    Microsoft::WRL::ComPtr<IShellLinkW> link;
    Microsoft::WRL::ComPtr<IPersistFile> file;
    return SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link)))
        && SUCCEEDED(link->SetPath(target.c_str())) && SUCCEEDED(link->SetArguments(arguments.c_str()))
        && SUCCEEDED(link->SetWorkingDirectory(directory.c_str()))
        && (icon.empty() || SUCCEEDED(link->SetIconLocation(icon.c_str(), 0)))
        && SUCCEEDED(link.As(&file)) && SUCCEEDED(file->Save(path.c_str(), TRUE));
}
static uint32_t CallAction(uint32_t action, const Json& request, const std::string* paths, Json& reply, uint32_t& commit) {
    const auto input = automation::WriteJson(request);
    SetupBridgeBuffer buffer{};
    const auto code = setup_bridge_action(action, reinterpret_cast<const uint8_t*>(input.data()), input.size(),
        paths ? reinterpret_cast<const uint8_t*>(paths->data()) : nullptr, paths ? paths->size() : 0, 0, &buffer, &commit);
    memory_fault::armed = false;
    reply = buffer.data ? automation::ParseJson({reinterpret_cast<char*>(buffer.data), buffer.length}, MaxResponseBytes) : Json{};
    memory_fault::watchedAllocation = buffer.data; memory_fault::watchedReleased = false;
    const bool hadBuffer = buffer.data != nullptr;
    setup_bridge_free(&buffer);
    if (hadBuffer && !memory_fault::watchedReleased) throw automation::Invalid("response-leak");
    return code;
}
static int CheckRootIcons(const setup::Paths& paths, const std::wstring& other, bool same, PCWSTR payload) {
    std::wstring folder;
    REQUIRE(setup::EnsureCompanionFolder(paths, folder));
    const auto script = setup::Join(folder, L"plain.ps1");
    const auto icon = setup::Join(other, L"test.ico"), exe = setup::Join(other, L"icon-source.exe");
    Put(script, "# never executed"); Put(setup::Join(folder, L"type.exe"), "");
    PutTestIcon(icon);
    REQUIRE(CopyFileW(payload, exe.c_str(), TRUE));
    REQUIRE(SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)));
    const bool linked = SaveTestLink(setup::Join(folder, L"custom.lnk"), script, icon)
        && SaveTestLink(setup::Join(folder, L"target.lnk"), exe, L"");
    CoUninitialize();
    REQUIRE(linked);
    const auto locations = BridgeTestPathJson(paths);
    Json list; uint32_t commit = 0;
    REQUIRE(CallAction(SETUP_LIST_PROGRAMS, Json::Object(), &locations, list, commit) == 0 && !commit);
    std::map<std::wstring, long long> icons;
    for (const auto& row : list.At(L"entries").array) {
        REQUIRE(row.At(L"status").text == L"ordinary");
        icons.emplace(row.At(L"name").text, row.At(L"icon").Integer(0, 1024));
    }
    // Within the root, both resources load. Outside it, each row reuses its file-type icon.
    REQUIRE((icons.at(L"custom.lnk") != icons.at(L"plain.ps1")) == same);
    REQUIRE((icons.at(L"target.lnk") != icons.at(L"type.exe")) == same);
    return 0;
}
static int CheckRootPair(const std::wstring& root, const std::wstring& other, bool same, PCWSTR payload) {
    const auto paths = BridgeTestPaths(root), peer = BridgeTestPaths(other);
    REQUIRE(CheckRootIcons(paths, other, same, payload) == 0);
    const auto locations = BridgeTestPathJson(paths);
    auto pick = Json::Object(); pick[L"testSelection"] = Json(other);
    Json reply; uint32_t commit = 0;
    const uint32_t selectionCode = same ? ERROR_SUCCESS : ERROR_INVALID_DATA;
    REQUIRE(CallAction(SETUP_DIRECTORY, pick, &locations, reply, commit) == selectionCode);
    if (same) REQUIRE(reply.At(L"path").text == other);
    setup::Options options;
    options.directory = other; options.codexEnv = false;
    if (!same) {
        REQUIRE(CallAction(SETUP_SAVE, Options(options), &locations, reply, commit) == ERROR_INVALID_DATA && !commit);
        REQUIRE(GetFileAttributesW(setup::Join(other, setup::LauncherName).c_str()) == INVALID_FILE_ATTRIBUTES);
        options.directory = root;
    }
    // Hold a real GUI save at the entry to Apply while the headless entry attempts a deployment.
    const auto request = Options(options);
    const auto resultPath = setup::Join(other, L"concurrent-result.json");
    const auto tail = L"--headless deploy --base defaults --startup off --desktop off --codex-env leave --official-startup leave --directory "
        + automation::Quote(other) + L" --result " + automation::Quote(resultPath);
    automation::Handle entered(CreateEventW(nullptr, TRUE, FALSE, nullptr)), resume(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    REQUIRE(entered && resume);
    saveEntered = entered.value; continueSave = resume.value;
    uint32_t saved = ERROR_GEN_FAILURE;
    std::thread writer([&] {
        try { saved = CallAction(SETUP_SAVE, request, &locations, reply, commit); }
        catch (...) { saved = ERROR_INVALID_DATA; }
    });
    const bool ready = WaitForSingleObject(entered.value, 10000) == WAIT_OBJECT_0;
    const int exitCode = ready ? setup::RunSetupCommandLine(GetModuleHandleW(nullptr), tail, &peer, IsolatedStartup) : -1;
    SetEvent(resume.value); writer.join(); saveEntered = continueSave = nullptr;
    REQUIRE(ready && saved == 0 && commit == 1 && exitCode == (same ? 31 : 0));
    const auto result = automation::ParseJson(automation::ReadBytes(resultPath));
    REQUIRE(result.At(L"code").text == (same ? L"busy" : L"deployed") && result.At(L"committed").Bool() == !same);
    REQUIRE(GetFileAttributesW(setup::Join(root, setup::LauncherName).c_str()) != INVALID_FILE_ATTRIBUTES);
    REQUIRE(GetFileAttributesW(setup::Join(other, setup::LauncherName).c_str()) != INVALID_FILE_ATTRIBUTES);
    return 0;
}
static int CheckRedirectedImport(PCWSTR root) {
    const auto paths = BridgeTestPaths(root);
    const auto attributes = GetFileAttributesW(paths.defaultDirectory.c_str());
    REQUIRE(attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT));
    const auto source = setup::Join(root, L"sample.exe");
    REQUIRE(GetFileAttributesW(source.c_str()) != INVALID_FILE_ATTRIBUTES);
    auto pick = Json::Object(); pick[L"testSelection"] = Json(source);
    const auto request = automation::WriteJson(pick), locations = BridgeTestPathJson(paths);
    SetupBridgeBuffer output{};
    uint32_t commit = 0;
    const auto code = setup_bridge_action(SETUP_ADD_PROGRAM, reinterpret_cast<const uint8_t*>(request.data()), request.size(),
        reinterpret_cast<const uint8_t*>(locations.data()), locations.size(), 0, &output, &commit);
    const bool empty = !output.data && !output.length;
    setup_bridge_free(&output);
    REQUIRE(code == ERROR_INVALID_DATA && commit == 0 && empty);
    return 0;
}
int wmain(int argc, wchar_t** argv) {
    try {
        l10n::Select(L"zh-CN");
        if (argc == 5 && !wcscmp(argv[1], L"--check-root-alias")) return CheckRootPair(argv[2], argv[3], true, argv[4]);
        if (argc == 5 && !wcscmp(argv[1], L"--check-root-siblings")) return CheckRootPair(argv[2], argv[3], false, argv[4]);
        if (argc != 3) return 2;
        if (!wcscmp(argv[1], L"--check-redirected-import")) return CheckRedirectedImport(argv[2]);
        const auto root = setup::Join(argv[1], L"bridge-actions-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
        REQUIRE(CreateDirectoryW(root.c_str(), nullptr));
        auto paths = BridgeTestPaths(root);
        const auto pathJson = BridgeTestPathJson(paths);
        setup::Options options; options.directory = paths.defaultDirectory; options.proxy = true; options.port = L"7890";
        options.extra = L"\"\" \"中文 参数\" \"quote\\\"end\"";
        Json reply;
        uint32_t commit = 0;
        auto call = [&](uint32_t action, const Json& request, bool useSystemPaths = false) {
            return CallAction(action, request, useSystemPaths ? nullptr : &pathJson, reply, commit);
        };
        REQUIRE(call(SETUP_LIST_PROGRAMS, Json::Object()) == 0 && commit == 0);
        REQUIRE(reply.At(L"entries").array.empty() && !reply.At(L"incomplete").Bool());
        auto arbitrary = Json::Object(); arbitrary[L"path"] = Json(root);
        REQUIRE(call(SETUP_LIST_PROGRAMS, arbitrary) != 0);
        REQUIRE(call(SETUP_VALIDATE, Options(options)) == 0 && reply.At(L"valid").Bool());
        REQUIRE(reply.At(L"arguments").String().find(options.extra) != std::wstring::npos);
        const auto preview = automation::WriteJson(reply);
        // Even without Windows folders or an STA, preview must return the same result.
        REQUIRE(SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)));
        const auto previewCode = call(SETUP_VALIDATE, Options(options), true);
        CoUninitialize();
        REQUIRE(previewCode == 0 && commit == 0 && automation::WriteJson(reply) == preview);
        for (const auto& bad : {L"\"unclosed", L"bad\nline"}) {
            auto invalid = options; invalid.extra = bad;
            REQUIRE(call(SETUP_VALIDATE, Options(invalid)) == 0 && !reply.At(L"valid").Bool());
        }
        auto invalid = options; invalid.extra.assign(setup::MaxArguments + 1, L'x');
        REQUIRE(call(SETUP_VALIDATE, Options(invalid)) == 0 && !reply.At(L"valid").Bool());
        invalid = options; invalid.port = L"65536";
        REQUIRE(call(SETUP_VALIDATE, Options(invalid)) == 0 && !reply.At(L"valid").Bool());
        REQUIRE(reply.At(L"nativeError").Integer(0, MAXDWORD) == ERROR_INVALID_PARAMETER);
        auto detection = Json::Object(); detection[L"protocol"] = Json(L"http"); detection[L"host"] = Json(L"127.0.0.1");
        REQUIRE(call(SETUP_DETECT, detection) == 0 && reply.At(L"port").String() == L"7897" && !reply.At(L"source").String().empty());
        detection[L"protocol"] = Json(L"https");
        REQUIRE(call(SETUP_DETECT, detection) == 0 && reply.At(L"port").String().empty() && reply.At(L"source").String().empty() && !reply.At(L"message").String().empty());
        auto malformed = Options(options); malformed[L"startup"] = Json(L"true");
        REQUIRE(call(SETUP_SAVE, malformed) != 0 && commit == 0);
        malformed = Options(options); malformed[L"extraKey"] = Json(true);
        REQUIRE(call(SETUP_SAVE, malformed) != 0 && commit == 0);
        REQUIRE(GetFileAttributesW(paths.defaultDirectory.c_str()) == INVALID_FILE_ATTRIBUTES);
        auto legacyRequest = Options(options);
        legacyRequest.object.erase(L"cloudCodex");
        REQUIRE(call(SETUP_SAVE, legacyRequest) == 0 && commit == 1);
        REQUIRE(reply.At(L"options").At(L"cloudCodex").Bool());
        REQUIRE(reply.At(L"commit").String() == L"saved");
        REQUIRE(reply.At(L"tasks").array.size() == 1 && reply.At(L"tasks").array[0].String() == L"restart");
        auto env = automation::ReadBytes(paths.codexEnvFile);
        REQUIRE(env.find("7890") != std::string::npos);
        // Follow the normal save and reload paths for both independent choices.
        const std::string sentinel = "KEEP_MATRIX=yes\r\n";
        for (const bool writeEnv : {false, true}) {
            for (const bool cloud : {false, true}) {
                auto selected = options;
                selected.codexEnv = writeEnv;
                selected.cloudCodex = cloud;
                Put(paths.codexEnvFile, sentinel);
                REQUIRE(call(SETUP_SAVE, Options(selected)) == 0 && commit == 1);
                REQUIRE(reply.At(L"tasks").array.empty() == !writeEnv);
                const auto bytes = automation::ReadBytes(paths.codexEnvFile);
                REQUIRE(writeEnv ? bytes.find("HTTPS_PROXY=http://127.0.0.1:7890") != bytes.npos : bytes == sentinel);
                REQUIRE(bytes.find("NODE_USE_ENV_PROXY") == bytes.npos);
                SetupBridgeBuffer buffer{};
                REQUIRE(setup_bridge_read_state(reinterpret_cast<const uint8_t*>(pathJson.data()), pathJson.size(), &buffer) == 0);
                const auto state = automation::ParseJson({reinterpret_cast<const char*>(buffer.data), buffer.length});
                setup_bridge_free(&buffer);
                REQUIRE(state.At(L"options").At(L"cloudCodex").Bool() == cloud);
                REQUIRE(state.At(L"options").At(L"codexEnv").Bool() == writeEnv);
                REQUIRE(state.At(L"options").At(L"extra").String() == options.extra);
            }
        }
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
        std::thread holder([&] { automation::UserLock lock; lock.AcquireForSetup(paths, true); SetEvent(entered); WaitForSingleObject(release, 10000); });
        WaitForSingleObject(entered, 10000);
        const auto blocked = call(SETUP_SAVE, Options(options));
        SetEvent(release); holder.join(); CloseHandle(entered); CloseHandle(release);
        REQUIRE(blocked == ERROR_BUSY && commit == 0);
        // Native selection imports the original .lnk bytes, including quoted arguments.
        const auto target = setup::Join(root, L"tool.exe"), shortcut = setup::Join(root, L"工具.lnk");
        REQUIRE(CopyFileW(argv[2], target.c_str(), TRUE));
        REQUIRE(SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)));
        const auto iconPath = setup::Join(root, L"custom.ico");
        PutTestIcon(iconPath);
        REQUIRE(SaveTestLink(shortcut, target, iconPath, options.extra, root));
        CoUninitialize();
        auto pick = Json::Object(); pick[L"testSelection"] = Json(shortcut);
        REQUIRE(call(SETUP_ADD_PROGRAM, pick) == 0 && commit == 1);
        REQUIRE(reply.At(L"entry").At(L"path").String() == shortcut);
        REQUIRE(reply.At(L"entry").At(L"arguments").String() == options.extra);
        REQUIRE(automation::ReadBytes(shortcut) == automation::ReadBytes(setup::Join(setup::Join(paths.defaultDirectory, companions::Folder), L"工具.lnk")));
        REQUIRE(call(SETUP_ADD_PROGRAM, pick) == 0); // Idempotent import.
        const auto folder = setup::Join(paths.defaultDirectory, companions::Folder);
        const auto registry = setup::Join(paths.defaultDirectory, automation::contract::RegistryFolder);
        REQUIRE(CreateDirectoryW(registry.c_str(), nullptr));
        const auto manifest = setup::Join(root, L"tool.json");
        auto spec = automation::ParseJson(R"({"schemaVersion":1,"id":"org.example.tool","name":"Example Tool","version":"1.0.0","requires":[],"entry":{"runtime":"exe","path":"tool.exe"},"readiness":{"mode":"exit-code"}})");
        Put(manifest, automation::WriteJson(spec));
        auto record = automation::ParseJson(R"({"schemaVersion":1,"managedBy":"ChatGPTProxyLauncher","id":"org.example.tool","overrides":{}})");
        record[L"manifestPath"] = Json(manifest);
        const auto registration = setup::Join(registry, L"org.example.tool.json");
        Put(registration, automation::WriteJson(record));
        Put(setup::Join(folder, L"ordinary.ps1"), "# never executed");
        Put(setup::Join(folder, L"unregistered.json"), automation::WriteJson(spec));
        const auto nested = setup::Join(folder, L"nested");
        REQUIRE(CreateDirectoryW(nested.c_str(), nullptr));
        Put(setup::Join(nested, L"nested.ps1"), "# never executed");
        Put(setup::Join(registry, L"org.example.broken.json"), "not JSON");
        auto listed = [&](PCWSTR name, PCWSTR status) {
            const auto& rows = reply.At(L"entries").array;
            return std::any_of(rows.begin(), rows.end(), [&](const auto& row) {
                return row.At(L"name").text == name && row.At(L"status").text == status;
            });
        };
        REQUIRE(call(SETUP_LIST_PROGRAMS, Json::Object()) == 0 && commit == 0 && !reply.At(L"incomplete").Bool());
        REQUIRE(reply.At(L"entries").array.size() == 3 && listed(L"Example Tool", L"adapted")
            && listed(L"ordinary.ps1", L"ordinary") && listed(L"org.example.broken", L"invalid"));
        const auto imageOf = [&](PCWSTR name) -> std::wstring {
            const auto& icons = reply.At(L"icons").array;
            for (const auto& row : reply.At(L"entries").array) {
                if (row.At(L"name").text == name && row.Has(L"icon"))
                    return icons.at(static_cast<size_t>(row.At(L"icon").Integer(0, 1024))).text;
            }
            return {};
        };
        const auto exeImage = imageOf(L"Example Tool"), typeImage = imageOf(L"ordinary.ps1");
        REQUIRE(exeImage.find(L"iVBORw0KGgo") == 0 && typeImage.find(L"iVBORw0KGgo") == 0 && exeImage != typeImage);
        unprobedTarget = manifest;
        simulatedDrive = setup::ui::ProgramDrive::Unverified;
        REQUIRE(call(SETUP_LIST_PROGRAMS, Json::Object()) == 0 && reply.At(L"incomplete").Bool());
        REQUIRE(listed(L"org.example.tool", L"unavailable") && listed(L"工具.lnk", L"ordinary") && !targetProbes);
        unprobedTarget.clear();
        const auto policy = setup::Join(paths.defaultDirectory, automation::contract::PolicyFile);
        Put(policy, "invalid policy");
        unprobedTarget = target;
        REQUIRE(call(SETUP_LIST_PROGRAMS, Json::Object()) == 0 && reply.At(L"incomplete").Bool());
        REQUIRE(listed(L"工具.lnk", L"ordinary") && listed(L"Example Tool", L"invalid") && !targetProbes);
        simulatedDrive = setup::ui::ProgramDrive::Missing;
        REQUIRE(call(SETUP_LIST_PROGRAMS, Json::Object()) == 0 && listed(L"工具.lnk", L"unavailable") && !targetProbes);
        unprobedTarget.clear();
        Put(policy, R"({"schemaVersion":1,"budgetMs":30000,"feedback":"quiet"})");
        auto second = spec, other = record;
        second[L"id"] = Json(L"org.example.other"); second[L"name"] = Json(L"Other Tool");
        second[L"dependsOn"] = automation::ParseJson(R"([{"id":"org.example.tool"}])");
        const auto secondManifest = setup::Join(root, L"other.json");
        Put(secondManifest, automation::WriteJson(second));
        other[L"id"] = Json(L"org.example.other"); other[L"manifestPath"] = Json(secondManifest);
        const auto secondRegistration = setup::Join(registry, L"org.example.other.json");
        Put(secondRegistration, automation::WriteJson(other));
        for (const auto dependency : {L"org.example.missing", L"org.example.broken"}) {
            auto needs = spec;
            auto deps = second.At(L"dependsOn");
            deps.array[0][L"id"] = Json(dependency);
            needs[L"dependsOn"] = std::move(deps);
            Put(manifest, automation::WriteJson(needs));
            REQUIRE(call(SETUP_LIST_PROGRAMS, Json::Object()) == 0 && !reply.At(L"incomplete").Bool());
            REQUIRE(listed(L"Example Tool", L"dependency-blocked") && listed(L"Other Tool", L"dependency-blocked"));
            REQUIRE(!listed(L"工具.lnk", L"ordinary")); // The runner still suppresses this duplicate.
        }
        Put(manifest, automation::WriteJson(spec));
        auto versioned = second;
        versioned[L"dependsOn"].array[0][L"minimumVersion"] = Json(L"1.0.1");
        Put(secondManifest, automation::WriteJson(versioned));
        REQUIRE(call(SETUP_LIST_PROGRAMS, Json::Object()) == 0 && listed(L"Other Tool", L"dependency-blocked"));
        versioned[L"dependsOn"].array[0][L"minimumVersion"] = Json(L"1.0.0");
        Put(secondManifest, automation::WriteJson(versioned));
        REQUIRE(call(SETUP_LIST_PROGRAMS, Json::Object()) == 0 && listed(L"Other Tool", L"adapted"));
        auto disabled = record; disabled[L"overrides"][L"enabled"] = Json(false);
        Put(registration, automation::WriteJson(disabled));
        REQUIRE(call(SETUP_LIST_PROGRAMS, Json::Object()) == 0 && listed(L"Other Tool", L"dependency-blocked"));
        Put(registration, automation::WriteJson(record));
        unprobedTarget = manifest; simulatedDrive = setup::ui::ProgramDrive::Unverified;
        REQUIRE(call(SETUP_LIST_PROGRAMS, Json::Object()) == 0 && reply.At(L"incomplete").Bool());
        REQUIRE(listed(L"Other Tool", L"adapted") && listed(L"工具.lnk", L"ordinary") && !targetProbes);
        unprobedTarget.clear();
        auto cyclic = spec;
        cyclic[L"dependsOn"] = automation::ParseJson(R"([{"id":"org.example.other"}])");
        Put(manifest, automation::WriteJson(cyclic));
        REQUIRE(call(SETUP_LIST_PROGRAMS, Json::Object()) == 0 && reply.At(L"incomplete").Bool());
        REQUIRE(listed(L"工具.lnk", L"ordinary") && listed(L"Example Tool", L"invalid"));
        REQUIRE(DeleteFileW(secondRegistration.c_str()));
        Put(manifest, automation::WriteJson(spec));
        record[L"overrides"][L"enabled"] = Json(false); Put(registration, automation::WriteJson(record));
        REQUIRE(call(SETUP_LIST_PROGRAMS, Json::Object()) == 0 && reply.At(L"entries").array.size() == 4);
        REQUIRE(listed(L"Example Tool", L"adapted-disabled") && listed(L"工具.lnk", L"ordinary"));
        const auto linkImage = imageOf(L"工具.lnk");
        REQUIRE(linkImage.find(L"iVBORw0KGgo") == 0 && linkImage != exeImage && imageOf(L"Example Tool") == exeImage);
        REQUIRE(MoveFileW(iconPath.c_str(), setup::Join(root, L"moved.ico").c_str()));
        REQUIRE(call(SETUP_LIST_PROGRAMS, Json::Object()) == 0 && imageOf(L"工具.lnk") == exeImage);
        // Disabled records on an offline drive need no package reads to remain visible.
        unprobedTarget = manifest;
        simulatedDrive = setup::ui::ProgramDrive::Unverified;
        REQUIRE(call(SETUP_LIST_PROGRAMS, Json::Object()) == 0 && listed(L"org.example.tool", L"disabled"));
        unprobedTarget.clear();
        auto hosted = spec;
        hosted[L"entry"][L"runtime"] = Json(L"powershell");
        hosted[L"entry"][L"path"] = Json(L"disabled.ps1");
        hosted[L"entry"][L"hostPath"] = Json(target);
        Put(setup::Join(root, L"disabled.ps1"), "# never executed");
        Put(manifest, automation::WriteJson(hosted));
        REQUIRE(call(SETUP_LIST_PROGRAMS, Json::Object()) == 0 && listed(L"Example Tool", L"adapted-disabled"));
        unprobedTarget = target;
        REQUIRE(call(SETUP_LIST_PROGRAMS, Json::Object()) == 0 && listed(L"org.example.tool", L"disabled") && !targetProbes);
        record[L"overrides"][L"enabled"] = Json(true); Put(registration, automation::WriteJson(record));
        REQUIRE(call(SETUP_LIST_PROGRAMS, Json::Object()) == 0 && reply.At(L"incomplete").Bool());
        REQUIRE(listed(L"org.example.tool", L"unavailable") && listed(L"工具.lnk", L"ordinary") && !targetProbes);
        record[L"overrides"][L"enabled"] = Json(false); Put(registration, automation::WriteJson(record));
        unprobedTarget.clear();
        spec[L"id"] = Json(L"org.example.different"); Put(manifest, automation::WriteJson(spec));
        REQUIRE(call(SETUP_LIST_PROGRAMS, Json::Object()) == 0 && listed(L"org.example.tool", L"disabled"));
        record[L"overrides"][L"enabled"] = Json(true); Put(registration, automation::WriteJson(record));
        REQUIRE(call(SETUP_LIST_PROGRAMS, Json::Object()) == 0 && listed(L"org.example.tool", L"invalid") && listed(L"工具.lnk", L"ordinary"));
        for (unsigned i = 0; i < 256; ++i) Put(setup::Join(folder, L"item-" + std::to_wstring(i) + L".ps1"), "");
        REQUIRE(call(SETUP_LIST_PROGRAMS, Json::Object()) == 0 && reply.At(L"incomplete").Bool() && listed(L"org.example.broken", L"invalid"));
        REQUIRE(reply.At(L"entries").array.size() >= 256 && reply.At(L"icons").array.size() < 8);
        pick[L"testSelection"] = Json(argv[2]);
        REQUIRE(call(SETUP_ADD_PROGRAM, pick) != 0 && commit == 0); // Outside harness root.
        pick[L"testSelection"] = Json(L"");
        REQUIRE(call(SETUP_DIRECTORY, pick) == 0 && reply.At(L"path").String().empty());
        auto prefs = Json::Object(); prefs[L"dark"] = Json(false); prefs[L"language"] = Json(L"ja-JP");
        REQUIRE(call(SETUP_PREFERENCES, prefs) == 0 && reply.At(L"preferences").At(L"resolvedLanguage").String() == L"ja-JP");
        prefs[L"language"] = Json(L"bad"); REQUIRE(call(SETUP_PREFERENCES, prefs) != 0);
        auto location = Json::Object(); location[L"location"] = Json(L"windows");
        auto addExe = Json::Object(); addExe[L"testSelection"] = Json(target);
        REQUIRE(call(SETUP_ADD_PROGRAM, addExe) == 0 && commit == 1);
        const auto originalLink = automation::ReadBytes(setup::CompanionShortcutPath(folder, target));
        const auto sameName = setup::Join(nested, L"tool.exe");
        REQUIRE(CopyFileW(target.c_str(), sameName.c_str(), TRUE));
        addExe[L"testSelection"] = Json(sameName);
        REQUIRE(call(SETUP_ADD_PROGRAM, addExe) == ERROR_ALREADY_EXISTS && !commit && reply.At(L"conflictDirectory").String() == folder);
        auto programFolder = Json::Object(); programFolder[L"location"] = Json(L"program-conflict");
        REQUIRE(call(SETUP_OPEN_LOCATION, programFolder) == 0 && !commit);
        programFolder[L"path"] = Json(root);
        REQUIRE(call(SETUP_OPEN_LOCATION, programFolder) == ERROR_INVALID_DATA);
        REQUIRE(automation::ReadBytes(setup::CompanionShortcutPath(folder, target)) == originalLink);
        REQUIRE(call(SETUP_OPEN_LOCATION, location) == 0); // No real Settings process.
        location[L"location"] = Json(L"cmd.exe"); REQUIRE(call(SETUP_OPEN_LOCATION, location) != 0);
        location[L"location"] = Json(L"conflict");
        REQUIRE(call(SETUP_OPEN_LOCATION, location) == ERROR_NOT_FOUND);
        REQUIRE(CopyFileW(shortcut.c_str(), paths.settingsLink.c_str(), FALSE));
        SetupBridgeBuffer conflict{};
        REQUIRE(setup_bridge_read_state(reinterpret_cast<const uint8_t*>(pathJson.data()), pathJson.size(), &conflict) == ERROR_ALREADY_EXISTS);
        const auto error = automation::ParseJson({reinterpret_cast<char*>(conflict.data), conflict.length});
        setup_bridge_free(&conflict);
        REQUIRE(error.At(L"nativeError").Integer(0, MAXDWORD) == ERROR_ALREADY_EXISTS
            && error.At(L"conflictDirectory").String() == paths.defaultDirectory && !error.At(L"message").String().empty());
        REQUIRE(call(SETUP_SAVE, Options(options)) == ERROR_ALREADY_EXISTS && !commit);
        REQUIRE(reply.At(L"conflictDirectory").String() == paths.defaultDirectory && reply.At(L"commit").String() == L"not-started");
        REQUIRE(automation::ReadBytes(paths.settingsLink) == automation::ReadBytes(shortcut));
        REQUIRE(call(SETUP_OPEN_LOCATION, location) == 0 && !commit);
        location[L"path"] = Json(root);
        REQUIRE(call(SETUP_OPEN_LOCATION, location) == ERROR_INVALID_DATA);
        puts("PASS bridge actions: native validation, commit states, committed response OOM, isolated save/env preservation, user lock, original shortcut import, cancellation, preferences and fixed locations");
        return 0;
    } catch (...) { memory_fault::armed = false; puts("FAIL bridge action exception"); return 50; }
}
