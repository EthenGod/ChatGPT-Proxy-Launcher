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
static PCWSTR collisionTarget = nullptr;
static BOOL WINAPI TestMove(PCWSTR from, PCWSTR to, DWORD flags) {
    if (++moveCalls == failMoveAt) { SetLastError(ERROR_ACCESS_DENIED); return FALSE; }
    if (collisionTarget && !_wcsicmp(to, collisionTarget)) {
        collisionTarget = nullptr;
        HANDLE h = CreateFileW(to, GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) return FALSE;
        DWORD written = 0;
        BOOL ok = WriteFile(h, "foreign", 7, &written, nullptr);
        CloseHandle(h);
        if (!ok || written != 7) return FALSE;
    }
    return MoveFileExW(from, to, flags);
}
#define MoveFileExW TestMove
#include "../src/SetupCore.cpp"
#undef MoveFileExW
#include "../src/SetupResources.h"
#include "../src/AppVersion.h"

#define REQUIRE(condition) do { if (!(condition)) { printf("FAIL at line %d: %s\n", __LINE__, #condition); return 20; } } while (0)
static int EnvTests() {
    using namespace setup;
    Options options; options.proxy = true; options.port = L"12345";
    std::vector<BYTE> merged, again;
    REQUIRE(MergeCodexEnv({}, options, merged));
    const std::string expected = "all_proxy=http://127.0.0.1:12345\r\nALL_PROXY=http://127.0.0.1:12345\r\nHTTP_PROXY=http://127.0.0.1:12345\r\nHTTPS_PROXY=http://127.0.0.1:12345\r\n";
    REQUIRE(std::string(merged.begin(), merged.end()) == expected);
    REQUIRE(MergeCodexEnv(merged, options, again) && again == merged);
    const std::string old = "\xef\xbb\xbf# keep comment\nOTHER=value\nTEXT=\"first\nHTTP_PROXY=literal\nlast\"\n export ALL_PROXY = 'http://old:1'  # keep inline\nHTTP_PROXY=http://old:1\nHTTP_PROXY=http://old:2\nexport\thttps_proxy=http://old:3\nTAIL=中文";
    options.protocol = L"socks5"; options.host = L"[::1]";
    REQUIRE(MergeCodexEnv({old.begin(), old.end()}, options, merged));
    const std::string text(merged.begin(), merged.end());
    REQUIRE(text.find("\xef\xbb\xbf# keep comment\nOTHER=value\nTEXT=\"first\nHTTP_PROXY=literal\nlast\"\n") == 0);
    REQUIRE(text.find(" export ALL_PROXY =socks5://[::1]:12345  # keep inline\n") != std::string::npos);
    REQUIRE(text.find("old:") == std::string::npos && text.find("TAIL=中文\n") != std::string::npos);
    REQUIRE(text.find("export\thttps_proxy=socks5://[::1]:12345\n") != std::string::npos);
    REQUIRE(text.find("HTTPS_PROXY=socks5://[::1]:12345\n") != std::string::npos);
    REQUIRE(text.find('\r') == std::string::npos);
    REQUIRE(MergeCodexEnv(merged, options, again) && again == merged);
    options.protocol = L"https"; options.host = L"proxy.example.com";
    REQUIRE(MergeCodexEnv({}, options, merged));
    REQUIRE(std::string(merged.begin(), merged.end()).find("https://proxy.example.com:12345") != std::string::npos);
    REQUIRE(!MergeCodexEnv({0xff, 0xfe, 0, 0}, options, merged));
    const std::string broken = "TEXT=\"unterminated";
    REQUIRE(!MergeCodexEnv({broken.begin(), broken.end()}, options, merged));
    const std::string multilineProxy = "HTTP_PROXY=\"first\nlast\"";
    REQUIRE(!MergeCodexEnv({multilineProxy.begin(), multilineProxy.end()}, options, merged));
    // A bare CR must not cause the following unrelated variable to be swallowed as a proxy value.
    const std::string bareCr = "HTTP_PROXY=old\rKEEP=value\r";
    REQUIRE(!MergeCodexEnv({bareCr.begin(), bareCr.end()}, options, merged));
    REQUIRE(!MergeCodexEnv(std::vector<BYTE>(1024 * 1024 + 1, 'a'), options, merged));
    options.host = L"bad${VALUE}";
    REQUIRE(!MergeCodexEnv({}, options, merged));
    puts("PASS Codex env generation, UTF-8/BOM, multiline preservation, duplicate keys, idempotence and protocol selection");
    return 0;
}
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
    {
        const std::string expected = APP_VERSION_TEXT;
        const std::wstring current(expected.begin(), expected.end());
        REQUIRE(ReadFileVersion(launcherArg) == current && ReadFileVersion(setupArg) == current);
        const std::wstring base = Join(root, L"version 中文");
        REQUIRE(EnsureDirectory(base));
        Paths versionPaths{base, Join(base, L"startup.lnk"), Join(base, L"settings.lnk"), {}, {}};
        const auto file = Join(base, LauncherName);
        REQUIRE(WriteAtomic(file, payloadBytes.data));
        Loaded installed;
        REQUIRE(Load(versionPaths, installed) && !installed.deployed && installed.deployedVersion.empty());
        REQUIRE(WriteLink(versionPaths.settingsLink, {file, L"", base, Marker}));
        REQUIRE(Load(versionPaths, installed) && installed.deployed && installed.deployedVersion == current);
        // Give the isolated EXE a different numeric version, leaving its text version unchanged.
        HMODULE image = LoadLibraryExW(file.c_str(), nullptr, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
        REQUIRE(image);
        HRSRC version = FindResourceW(image, MAKEINTRESOURCEW(1), RT_VERSION);
        REQUIRE(version);
        const auto raw = static_cast<const BYTE*>(LockResource(LoadResource(image, version)));
        REQUIRE(raw);
        std::vector<BYTE> resourceBytes(raw, raw + SizeofResource(image, version));
        FreeLibrary(image);
        const BYTE signature[]{0xbd, 0x04, 0xef, 0xfe};
        const auto at = std::search(resourceBytes.begin(), resourceBytes.end(), std::begin(signature), std::end(signature));
        REQUIRE(at != resourceBytes.end() && static_cast<size_t>(resourceBytes.end() - at) >= sizeof(VS_FIXEDFILEINFO));
        VS_FIXEDFILEINFO fixed{}; CopyMemory(&fixed, &*at, sizeof(fixed));
        fixed.dwFileVersionMS = MAKELONG(3, 2); fixed.dwFileVersionLS = MAKELONG(5, 4);
        CopyMemory(&*at, &fixed, sizeof(fixed));
        HANDLE edit = BeginUpdateResourceW(file.c_str(), TRUE); REQUIRE(edit);
        const BOOL updated = UpdateResourceW(edit, RT_VERSION, MAKEINTRESOURCEW(1), MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
            resourceBytes.data(), static_cast<DWORD>(resourceBytes.size()));
        const BOOL committed = EndUpdateResourceW(edit, !updated); REQUIRE(updated && committed);
        REQUIRE(Load(versionPaths, installed) && installed.deployedVersion == L"2.3.4.5");
        // Old executables without version resources remain usable, with an unknown version.
        edit = BeginUpdateResourceW(file.c_str(), TRUE); REQUIRE(edit && EndUpdateResourceW(edit, FALSE));
        REQUIRE(Load(versionPaths, installed) && installed.deployed && installed.deployedVersion.empty());
        Options fresh; fresh.directory = base;
        std::wstring savedTarget;
        REQUIRE(Apply(versionPaths, fresh, payload, savedTarget));
        REQUIRE(Load(versionPaths, installed) && installed.deployedVersion == current);
        REQUIRE(WriteAtomic(file, {1, 2, 3, 4}));
        REQUIRE(Load(versionPaths, installed) && installed.deployed && installed.deployedVersion.empty());
        REQUIRE(DeleteFileW(file.c_str()));
        REQUIRE(Load(versionPaths, installed) && installed.hasSettings && !installed.deployed && installed.deployedVersion.empty());
        REQUIRE(CreateDirectoryW(file.c_str(), nullptr));
        REQUIRE(Load(versionPaths, installed) && !installed.deployed && installed.deployedVersion.empty());
        puts("PASS deployed version: shared build metadata, managed target only, different numeric version, versionless/corrupt/missing file and refresh after save");
    }
    // A file arriving after preflight belongs to someone else, even during first deployment.
    for (unsigned index = 0; index < 5; ++index) {
        const auto base = Join(root, L"late-conflict-" + std::to_wstring(index));
        Paths late{base, Join(base, L"startup.lnk"), Join(base, L"settings.lnk"), Join(base, L"desktop.lnk"), Join(base, L".env")};
        Options first; first.directory = base; first.startup = true; first.desktop = true;
        first.proxy = true; first.port = L"12345";
        const std::wstring destinations[] = {Join(base, LauncherName), late.settingsLink, late.startupLink, late.desktopLink, late.codexEnvFile};
        collisionTarget = destinations[index].c_str();
        std::wstring deployed;
        auto result = Apply(late, first, payload, deployed);
        REQUIRE(!collisionTarget && !result && result.conflictDirectory == base && deployed.empty());
        for (unsigned i = 0; i < 5; ++i) {
            Snapshot file; REQUIRE(ReadFileBytes(destinations[i], file));
            REQUIRE(i == index ? file.exists && file.data.size() == 7 && !memcmp(file.data.data(), "foreign", 7) : !file.exists);
        }
    }
    const Paths paths{ Join(root, L"settings"), Join(root, L"startup\\Managed.lnk"), Join(root, L"settings\\State.lnk"),
        Join(root, L"桌面 Desktop\\ChatGPT Proxy Launcher.lnk"), Join(root, L"profile\\.codex\\.env") };
    Loaded state;
    REQUIRE(Load(paths, state) && !state.hasSettings && state.options.startup && state.options.silent);
    REQUIRE(!state.options.desktop);
    Options options; options.directory = Join(root, L"部署 目录");
    options.startup = true; options.silent = true; options.proxy = true; options.port = L"7897";
    options.desktop = true;
    options.extra = L"--title=\"中文 空格\" \"\"";
    std::wstring target, arguments;
    REQUIRE(Apply(paths, options, payload, target));
    Snapshot envBefore, envAfter;
    REQUIRE(ReadFileBytes(paths.codexEnvFile, envBefore) && envBefore.exists);
    REQUIRE(std::string(envBefore.data.begin(), envBefore.data.end()).find("all_proxy=http://127.0.0.1:7897") == 0);
    options.codexEnv = false; options.port = L"7898";
    REQUIRE(Apply(paths, options, payload, target));
    REQUIRE(ReadFileBytes(paths.codexEnvFile, envAfter) && envAfter.data == envBefore.data);
    options.codexEnv = true; options.proxy = false;
    REQUIRE(Apply(paths, options, payload, target));
    REQUIRE(ReadFileBytes(paths.codexEnvFile, envAfter) && envAfter.data == envBefore.data);
    options.proxy = true; options.port = L"7897";
    REQUIRE(Apply(paths, options, payload, target));
    {
        Snapshot settings, startup, desktop, file;
        REQUIRE(ReadFileBytes(paths.settingsLink, settings) && ReadFileBytes(paths.startupLink, startup)
            && ReadFileBytes(paths.desktopLink, desktop));
        Options changed = options; changed.port = L"12345";
        changed.directory = Join(root, L"env-rollback-new");
        const std::string bareCr = "HTTP_PROXY=old\rKEEP=value\r";
        const std::vector<BYTE> invalidEnv(bareCr.begin(), bareCr.end());
        REQUIRE(WriteAtomic(paths.codexEnvFile, invalidEnv));
        moveCalls = 0;
        REQUIRE(!Apply(paths, changed, payload, target) && moveCalls == 0);
        REQUIRE(ReadFileBytes(paths.codexEnvFile, file) && file.data == invalidEnv);
        REQUIRE(WriteAtomic(paths.codexEnvFile, envBefore.data));
        // The EXE and all three shortcuts committed before the final env write fails.
        moveCalls = 0; failMoveAt = 5;
        REQUIRE(!Apply(paths, changed, payload, target)); failMoveAt = 0;
        REQUIRE(!Exists(Join(changed.directory, LauncherName)));
        REQUIRE(ReadFileBytes(paths.settingsLink, file) && file.data == settings.data);
        REQUIRE(ReadFileBytes(paths.startupLink, file) && file.data == startup.data);
        REQUIRE(ReadFileBytes(paths.desktopLink, file) && file.data == desktop.data);
        REQUIRE(ReadFileBytes(paths.codexEnvFile, file) && file.data == envBefore.data);
        // A read lock permits preflight but blocks atomic replacement: all link changes roll back.
        changed.directory = options.directory;
        HANDLE envLock = CreateFileW(paths.codexEnvFile.c_str(), GENERIC_READ, FILE_SHARE_READ,
            nullptr, OPEN_EXISTING, 0, nullptr);
        REQUIRE(envLock != INVALID_HANDLE_VALUE);
        const Result locked = Apply(paths, changed, payload, target); CloseHandle(envLock);
        REQUIRE(!locked);
        REQUIRE(ReadFileBytes(paths.settingsLink, file) && file.data == settings.data);
        REQUIRE(ReadFileBytes(paths.startupLink, file) && file.data == startup.data);
        REQUIRE(ReadFileBytes(paths.desktopLink, file) && file.data == desktop.data);
        REQUIRE(ReadFileBytes(paths.codexEnvFile, file) && file.data == envBefore.data);
        Paths unavailable = paths; unavailable.codexEnvFile.clear();
        REQUIRE(!Apply(unavailable, options, payload, target));
        Options skipped = options; skipped.codexEnv = false;
        REQUIRE(Apply(unavailable, skipped, payload, target));
        skipped.codexEnv = true; skipped.proxy = false;
        REQUIRE(Apply(unavailable, skipped, payload, target));
        REQUIRE(ReadFileBytes(paths.codexEnvFile, file) && file.data == envBefore.data);
        REQUIRE(Apply(paths, options, payload, target));
        puts("PASS isolated Codex env opt-out, missing profile, locked file, full rollback and late-file collision protection");
    }
    REQUIRE(Load(paths, state) && state.deployed && state.options.startup);
    REQUIRE(state.options.directory == options.directory && state.options.extra == options.extra);
    REQUIRE(BuildArguments(options, arguments));
    Link link;
    REQUIRE(ReadLink(paths.startupLink, link) && link.arguments == arguments && link.target == target && link.directory == options.directory);
    REQUIRE(state.options.desktop);
    REQUIRE(ReadLink(paths.desktopLink, link) && link.arguments == arguments && link.target == target && link.directory == options.directory);
    {
        Com<IShellLinkW> shell; Com<IPersistFile> persist;
        REQUIRE(SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&shell.p))));
        REQUIRE(SUCCEEDED(shell->QueryInterface(IID_PPV_ARGS(&persist.p))));
        REQUIRE(SUCCEEDED(persist->Load(paths.desktopLink.c_str(), STGM_READ)));
        wchar_t icon[MAX_PATH]{}; int index = -1;
        REQUIRE(SUCCEEDED(shell->GetIconLocation(icon, MAX_PATH, &index)) && target == icon && index == 0);
    }
    Snapshot disk;
    REQUIRE(ReadFileBytes(target, disk) && disk.data == payloadBytes.data);
    options.startup = false;
    REQUIRE(Apply(paths, options, payload, target));
    REQUIRE(!Exists(paths.startupLink));
    REQUIRE(ReadLink(paths.desktopLink, link) && link.target == target);
    REQUIRE(Load(paths, state) && state.deployed && !state.options.startup && state.options.extra == options.extra);
    options.startup = true; options.directory = Join(root, L"新目录 new");
    const std::wstring oldTarget = state.managedTarget;
    REQUIRE(Apply(paths, options, payload, target));
    REQUIRE(Exists(oldTarget)); // Never sweep or remove an old deployment directory.
    REQUIRE(ReadLink(paths.startupLink, link) && link.target == target);
    REQUIRE(ReadLink(paths.desktopLink, link) && link.target == target && link.directory == options.directory);
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
    options = state.options;
    Snapshot desktopBefore;
    REQUIRE(ReadFileBytes(paths.desktopLink, desktopBefore));
    options.extra += L" --desktop-test";
    moveCalls = 0; failMoveAt = 3; // Settings and startup succeeded; desktop commit fails.
    REQUIRE(!Apply(paths, options, payload, target)); failMoveAt = 0;
    REQUIRE(ReadFileBytes(paths.settingsLink, disk) && disk.data == settingsBefore.data);
    REQUIRE(ReadFileBytes(paths.startupLink, disk) && disk.data == startupBefore.data);
    REQUIRE(ReadFileBytes(paths.desktopLink, disk) && disk.data == desktopBefore.data);
    options.directory = Join(root, L"desktop-rollback-new");
    moveCalls = 0; failMoveAt = 4; // A new EXE was written before the three link commits.
    REQUIRE(!Apply(paths, options, payload, target)); failMoveAt = 0;
    REQUIRE(!Exists(Join(options.directory, LauncherName)));
    REQUIRE(ReadFileBytes(paths.settingsLink, disk) && disk.data == settingsBefore.data);
    REQUIRE(ReadFileBytes(paths.startupLink, disk) && disk.data == startupBefore.data);
    REQUIRE(ReadFileBytes(paths.desktopLink, disk) && disk.data == desktopBefore.data);
    options = state.options; options.desktop = false;
    lock = CreateFileW(paths.desktopLink.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    REQUIRE(lock != INVALID_HANDLE_VALUE);
    REQUIRE(!Apply(paths, options, payload, target)); CloseHandle(lock);
    REQUIRE(ReadFileBytes(paths.settingsLink, disk) && disk.data == settingsBefore.data);
    REQUIRE(ReadFileBytes(paths.startupLink, disk) && disk.data == startupBefore.data);
    REQUIRE(ReadFileBytes(paths.desktopLink, disk) && disk.data == desktopBefore.data);
    REQUIRE(Apply(paths, options, payload, target));
    REQUIRE(!Exists(paths.desktopLink) && Exists(paths.startupLink));
    REQUIRE(Load(paths, state) && !state.options.desktop);
    options.desktop = true;
    REQUIRE(Apply(paths, options, payload, target));
    REQUIRE(Load(paths, state) && state.options.desktop);
    REQUIRE(ReadLink(paths.desktopLink, link));
    link.description = L"Another tool";
    REQUIRE(WriteLink(paths.desktopLink, link));
    REQUIRE(ReadFileBytes(paths.desktopLink, desktopBefore));
    Result desktopConflict = Apply(paths, options, payload, target);
    REQUIRE(!desktopConflict && desktopConflict.conflictDirectory == Parent(paths.desktopLink));
    options.desktop = false; // An unrelated desktop link must not block opting out.
    REQUIRE(Apply(paths, options, payload, target));
    REQUIRE(ReadFileBytes(paths.desktopLink, disk) && disk.data == desktopBefore.data);
    REQUIRE(Load(paths, state) && !state.options.desktop);
    Paths unavailableDesktop = paths; unavailableDesktop.desktopLink.clear();
    REQUIRE(Apply(unavailableDesktop, options, payload, target));
    options.desktop = true;
    REQUIRE(!Apply(unavailableDesktop, options, payload, target));
    options.desktop = false;
    printf("PASS desktop shortcut opt-in, icon, argument sync, migration, opt-out, ownership and rollback (isolated folders)\n");
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
    int result = EnvTests();
    if (!result) result = ParameterTests();
    if (!result) result = IntegrationTests(argv[1], argv[2], argv[3]);
    CoUninitialize();
    return result;
}
