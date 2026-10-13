// SPDX-License-Identifier: GPL-3.0-only
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <string>
#include <vector>

// Fault injection exists only in this translation unit, never in the release binary.
static unsigned moveCalls = 0, failMoveAt = 0;
static BOOL WINAPI TestMove(PCWSTR from, PCWSTR to, DWORD flags) {
    if (++moveCalls == failMoveAt) { SetLastError(ERROR_ACCESS_DENIED); return FALSE; }
    return MoveFileExW(from, to, flags);
}
#define MoveFileExW TestMove
#include "../src/SetupCore.cpp"
#undef MoveFileExW
#include "../src/SetupResources.h"

#define REQUIRE(condition) do { if (!(condition)) { printf("FAIL at line %d: %s\n", __LINE__, #condition); return 20; } } while (0)
static int ParameterTests() {
    using namespace setup;
    Options options;
    std::wstring args, directory;
    REQUIRE(BuildArguments(options, args) && args.empty());
    options.proxy = true; options.port = L"7897"; options.silent = true;
    options.bypass = L"localhost;*.example.com";
    options.extra = L"--label=\"中文 空格\"  \"\" --literal=\"& | > %PATH%\"";
    REQUIRE(BuildArguments(options, args));
    REQUIRE(args == L"--proxy-server=http://127.0.0.1:7897 --no-startup-window --proxy-bypass-list=localhost;*.example.com --label=\"中文 空格\"  \"\" --literal=\"& | > %PATH%\"");
    Options parsed;
    REQUIRE(ParseArguments(args, parsed));
    REQUIRE(parsed.proxy && parsed.silent && parsed.protocol == L"http" && parsed.port == L"7897");
    REQUIRE(parsed.bypass == options.bypass);
    REQUIRE(parsed.extra == options.extra);
    std::vector<Token> originalTokens, restoredTokens;
    REQUIRE(Tokens(args, originalTokens));
    REQUIRE(BuildArguments(parsed, args));
    REQUIRE(Tokens(args, restoredTokens));
    REQUIRE(originalTokens.size() == restoredTokens.size());
    for (size_t i = 0; i < originalTokens.size(); ++i) {
        REQUIRE(originalTokens[i].value == restoredTokens[i].value);
        REQUIRE(originalTokens[i].raw == restoredTokens[i].raw);
    }
    for (const std::wstring& sample : { L"", L"plain", L"two words", L"中文 引号\"尾部\\", L"\\\"", L"\"\"", L"ends with \\\\" }) {
        int count = 0;
        LPWSTR* argv = CommandLineToArgvW((L"stub " + QuoteArgument(sample)).c_str(), &count);
        REQUIRE(argv && count == 2);
        const bool matches = sample == argv[1]; LocalFree(argv); REQUIRE(matches);
    }
    unsigned seed = 7;
    const std::wstring alphabet = L"a b\\\"\t中";
    for (int i = 0; i < 500; ++i) {
        std::wstring sample;
        for (int n = 0; n < i % 31; ++n) { seed = seed * 1664525u + 1013904223u; sample += alphabet[seed % alphabet.size()]; }
        std::vector<Token> tokens;
        REQUIRE(Tokens(QuoteArgument(sample), tokens));
        REQUIRE(tokens.size() == 1 && tokens[0].value == sample);
    }
    options = {}; options.proxy = true;
    for (const auto& bad : { L"", L"0", L"65536", L"123456", L"abc", L"80 80" }) {
        options.port = bad; REQUIRE(!BuildArguments(options, args));
    }
    options.port = L"65535"; REQUIRE(BuildArguments(options, args));
    options.host = L"[::1]"; options.protocol = L"socks5"; REQUIRE(BuildArguments(options, args));
    REQUIRE(ParseArguments(args, parsed) && parsed.host == L"[::1]" && parsed.protocol == L"socks5");
    options.host = L"http://example.com"; REQUIRE(!BuildArguments(options, args));
    options = {}; options.extra = std::wstring(MaxArguments, L'x'); REQUIRE(BuildArguments(options, args));
    options.extra += L'x'; REQUIRE(!BuildArguments(options, args));
    for (const auto& bad : { L"--launcher-check", L"--test=\"unfinished", L"one\ntwo", L"--no-startup-window --no-startup-window" }) {
        options.extra = bad; REQUIRE(!BuildArguments(options, args));
    }
    options.extra = L"--proxy-server=http://127.0.0.1:80"; options.proxy = true; options.port = L"81";
    REQUIRE(!BuildArguments(options, args));
    options = {}; options.extra = L"--launcher-check-extra"; REQUIRE(BuildArguments(options, args));
    REQUIRE(ParseArguments(L"--unknown=\"中文 \\\"引号\\\"\" \"\"", parsed));
    REQUIRE(BuildArguments(parsed, args) && args == L"--unknown=\"中文 \\\"引号\\\"\" \"\"");
    REQUIRE(ParseArguments(L"--proxy-server=custom-route --no-startup-window", parsed));
    REQUIRE(!parsed.proxy && parsed.silent && parsed.extra == L"--proxy-server=custom-route");
    REQUIRE(ParseArguments(L"--proxy-bypass-list=\"\" -- --no-startup-window", parsed));
    REQUIRE(!parsed.silent);
    REQUIRE(BuildArguments(parsed, args) && args == L"--proxy-bypass-list=\"\" -- --no-startup-window");
    REQUIRE(ParseArguments(L"--proxy-server=http://127.0.0.1:80 --proxy-server=http://127.0.0.1:81", parsed));
    REQUIRE(!BuildArguments(parsed, args));
    REQUIRE(NormalizeDirectory(L"C:\\Launcher 中文\\", directory) && directory == L"C:\\Launcher 中文");
    REQUIRE(NormalizeDirectory(L"D:\\", directory) && directory == L"D:\\");
    for (const auto& bad : { L"relative", L"C:relative", L"\\\\server\\share", L"C:\\bad?name", L"C:\\NUL", L"C:\\folder.", L"C:\\folder " })
        REQUIRE(!NormalizeDirectory(bad, directory));
    REQUIRE(!NormalizeDirectory(L"C:\\" + std::wstring(250, L'x'), directory));
    printf("PASS parameter generation, import, quoting (500 randomized cases), validation and paths\n");
    return 0;
}

static int IntegrationTests(PCWSTR rootArg, PCWSTR launcherArg, PCWSTR setupArg) {
    using namespace setup;
    Snapshot payloadBytes;
    REQUIRE(ReadFileBytes(launcherArg, payloadBytes) && payloadBytes.exists);
    HMODULE module = LoadLibraryExW(setupArg, nullptr, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
    REQUIRE(module);
    HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(IDR_LAUNCHER), RT_RCDATA);
    REQUIRE(resource);
    HGLOBAL loadedResource = LoadResource(module, resource);
    const BYTE* data = static_cast<const BYTE*>(LockResource(loadedResource));
    REQUIRE(data && SizeofResource(module, resource) == payloadBytes.data.size());
    REQUIRE(memcmp(data, payloadBytes.data.data(), payloadBytes.data.size()) == 0);
    FreeLibrary(module);
    const Payload payload{ payloadBytes.data.data(), payloadBytes.data.size() };
    const std::wstring root = Join(rootArg, L"setup-tests-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
    REQUIRE(EnsureDirectory(root));
    const Paths paths{ Join(root, L"settings"), Join(root, L"startup\\Managed.lnk"), Join(root, L"settings\\State.lnk") };
    Loaded state;
    REQUIRE(Load(paths, state) && !state.hasSettings && state.options.startup && state.options.silent);
    Options options; options.directory = Join(root, L"部署 目录");
    options.startup = true; options.silent = true; options.proxy = true; options.port = L"7897";
    options.extra = L"--title=\"中文 空格\" \"\"";
    std::wstring target, arguments;
    REQUIRE(Apply(paths, options, payload, target));
    REQUIRE(Load(paths, state) && state.deployed && state.options.startup);
    REQUIRE(state.options.directory == options.directory && state.options.extra == options.extra);
    REQUIRE(BuildArguments(options, arguments));
    Link link;
    REQUIRE(ReadLink(paths.startupLink, link) && link.arguments == arguments && link.target == target && link.directory == options.directory);
    Snapshot disk;
    REQUIRE(ReadFileBytes(target, disk) && disk.data == payloadBytes.data);
    options.startup = false;
    REQUIRE(Apply(paths, options, payload, target));
    REQUIRE(!Exists(paths.startupLink));
    REQUIRE(Load(paths, state) && state.deployed && !state.options.startup && state.options.extra == options.extra);
    options.startup = true; options.directory = Join(root, L"新目录 new");
    const std::wstring oldTarget = state.managedTarget;
    REQUIRE(Apply(paths, options, payload, target));
    REQUIRE(Exists(oldTarget)); // Never sweep or remove an old deployment directory.
    REQUIRE(ReadLink(paths.startupLink, link) && link.target == target);
    // Import manually appended unknown options from the actual managed startup link.
    link.arguments += L" --manual=\"保留 此项\"";
    REQUIRE(WriteLink(paths.startupLink, link));
    REQUIRE(Load(paths, state) && state.options.extra.find(L"--manual") != std::wstring::npos);
    options = state.options;
    // A failed shortcut commit must restore the previous settings and leave the old startup link intact.
    Snapshot settingsBefore, startupBefore;
    REQUIRE(ReadFileBytes(paths.settingsLink, settingsBefore)); REQUIRE(ReadFileBytes(paths.startupLink, startupBefore));
    options.silent = false;
    moveCalls = 0; failMoveAt = 2;
    REQUIRE(!Apply(paths, options, payload, target)); failMoveAt = 0;
    REQUIRE(ReadFileBytes(paths.settingsLink, disk) && disk.data == settingsBefore.data);
    REQUIRE(ReadFileBytes(paths.startupLink, disk) && disk.data == startupBefore.data);
    REQUIRE(Load(paths, state) && state.options.silent);
    // New destination copied, settings committed, then shortcut failure: both preceding changes roll back.
    options.directory = Join(root, L"rollback-new");
    const std::wstring rollbackTarget = Join(options.directory, LauncherName);
    moveCalls = 0; failMoveAt = 3;
    REQUIRE(!Apply(paths, options, payload, target)); failMoveAt = 0;
    REQUIRE(!Exists(rollbackTarget));
    REQUIRE(ReadFileBytes(paths.settingsLink, disk) && disk.data == settingsBefore.data);
    REQUIRE(ReadFileBytes(paths.startupLink, disk) && disk.data == startupBefore.data);
    // Read-locked startup link prevents disabling it; previously saved settings are restored.
    options = state.options; options.startup = false;
    HANDLE lock = CreateFileW(paths.startupLink.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    REQUIRE(lock != INVALID_HANDLE_VALUE);
    REQUIRE(!Apply(paths, options, payload, target)); CloseHandle(lock);
    REQUIRE(ReadFileBytes(paths.settingsLink, disk) && disk.data == settingsBefore.data);
    REQUIRE(ReadFileBytes(paths.startupLink, disk) && disk.data == startupBefore.data);
    // Unknown executable and unrelated shortcut must remain byte-for-byte unchanged.
    options.directory = Join(root, L"foreign"); REQUIRE(EnsureDirectory(options.directory));
    const std::wstring foreign = Join(options.directory, LauncherName);
    const std::vector<BYTE> unrelated{ 1, 3, 5, 7 };
    REQUIRE(WriteAtomic(foreign, unrelated));
    Result conflict = Apply(paths, options, payload, target);
    REQUIRE(!conflict && conflict.code == ERROR_ALREADY_EXISTS && conflict.conflictDirectory == options.directory);
    REQUIRE(ReadFileBytes(foreign, disk) && disk.data == unrelated);
    options = state.options;
    link.description = L"Not owned by this tool";
    REQUIRE(WriteLink(paths.startupLink, link));
    REQUIRE(ReadFileBytes(paths.startupLink, startupBefore));
    conflict = Apply(paths, options, payload, target);
    REQUIRE(!conflict && conflict.conflictDirectory == Parent(paths.startupLink));
    REQUIRE(ReadFileBytes(paths.startupLink, disk) && disk.data == startupBefore.data);
    const Payload invalid{ unrelated.data(), unrelated.size() };
    const Result invalidResult = Apply(paths, options, invalid, target);
    REQUIRE(!invalidResult && invalidResult.conflictDirectory.empty());
    REQUIRE(WriteLink(paths.settingsLink, link));
    conflict = Load(paths, state);
    REQUIRE(!conflict && conflict.conflictDirectory == Parent(paths.settingsLink));
    // Real Shell round trip at the supported limit, including an empty argument.
    Link boundary{ oldTarget, std::wstring(MaxArguments - 3, L'x') + L" \"\"", Parent(oldTarget), Marker };
    const std::wstring boundaryLink = Join(root, L"boundary.lnk");
    REQUIRE(WriteLink(boundaryLink, boundary));
    REQUIRE(ReadLink(boundaryLink, link) && link.arguments == boundary.arguments);
    printf("PASS embedded payload equality, real deployment and Shell links, restore, migration, ownership, rollback and length round trip\n");
    printf("Test artifacts remain under work; no ChatGPT or real startup entry was launched or changed.\n");
    return 0;
}
int wmain(int argc, wchar_t** argv) {
    if (argc != 4) return 2;
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(com)) return 3;
    int result = ParameterTests();
    if (!result) result = IntegrationTests(argv[1], argv[2], argv[3]);
    CoUninitialize();
    return result;
}
