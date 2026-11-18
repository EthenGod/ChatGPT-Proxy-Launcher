// SPDX-License-Identifier: GPL-3.0-only
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <algorithm>
#include "MemoryFault.h"
#include "../src/AutomationIO.h"

// Fault injection exists only in this translation unit, never in the release binary.
static unsigned moveCalls = 0, failMoveAt = 0, failSecondMoveAt = 0;
static unsigned throwMoveAt = 0, exhaustMemoryAfterMove = 0;
static PCWSTR collisionTarget = nullptr;
static PCWSTR redirectedPath = nullptr;
static PCWSTR failDeletePath = nullptr, replaceOnDeleteFailure = nullptr;
static PCWSTR changeShortcutPath = nullptr, changeShortcutTrigger = nullptr;
struct ShellNotice {
    LONG event;
    UINT flags;
    wchar_t path[MAX_PATH];
    bool hasSecondItem;
    DWORD attributes;
    unsigned moves;
};
static ShellNotice shellNotices[16]{};
static size_t shellNoticeCount = 0;
// Never notify the real Shell from these tests. Fixed storage also works under OOM injection.
static void WINAPI TestNotify(LONG event, UINT flags, LPCVOID item1, LPCVOID item2) noexcept {
    if (shellNoticeCount < _countof(shellNotices)) {
        auto& notice = shellNotices[shellNoticeCount];
        notice.event = event; notice.flags = flags; notice.hasSecondItem = item2 != nullptr;
        notice.path[0] = L'\0';
        if (item1 && (flags & SHCNF_TYPE) == SHCNF_PATHW)
            wcsncpy_s(notice.path, static_cast<PCWSTR>(item1), _TRUNCATE);
        notice.attributes = GetFileAttributesW(notice.path);
        notice.moves = moveCalls;
    }
    ++shellNoticeCount;
}
static bool NoticeIs(size_t index, LONG event, const std::wstring& path) {
    if (index >= shellNoticeCount || index >= _countof(shellNotices)) return false;
    const auto& notice = shellNotices[index];
    return notice.event == event && notice.flags == (SHCNF_PATHW | SHCNF_FLUSHNOWAIT)
        && !notice.hasSecondItem && path == notice.path && notice.moves == moveCalls;
}
static void ReplaceFixture(PCWSTR path) {
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) throw std::bad_alloc();
    DWORD written = 0;
    const BOOL ok = WriteFile(h, "foreign", 7, &written, nullptr); CloseHandle(h);
    if (!ok || written != 7) throw std::bad_alloc();
}
namespace automation {
static void TestRemoveUnchangedFile(const std::wstring& path, std::string_view expected) {
    if (failDeletePath && !_wcsicmp(path.c_str(), failDeletePath)) {
        if (replaceOnDeleteFailure) { ReplaceFixture(replaceOnDeleteFailure); replaceOnDeleteFailure = nullptr; }
        throw IoError(ERROR_ACCESS_DENIED);
    }
    RemoveUnchangedFile(path, expected);
}
static void TestCheckPlainPath(const std::wstring& path) {
    if (redirectedPath && SamePath(path, redirectedPath)) throw Invalid("redirected-path");
    CheckPlainPath(path);
}
}
static BOOL WINAPI TestMove(PCWSTR from, PCWSTR to, DWORD flags) {
    if (++moveCalls == failMoveAt || moveCalls == failSecondMoveAt) { SetLastError(ERROR_ACCESS_DENIED); return FALSE; }
    if (moveCalls == throwMoveAt) throw std::bad_alloc();
    if (collisionTarget && !_wcsicmp(to, collisionTarget)) {
        collisionTarget = nullptr;
        HANDLE h = CreateFileW(to, GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) return FALSE;
        DWORD written = 0;
        BOOL ok = WriteFile(h, "foreign", 7, &written, nullptr);
        CloseHandle(h);
        if (!ok || written != 7) return FALSE;
    }
    const BOOL moved = MoveFileExW(from, to, flags);
    if (moved && changeShortcutPath && !_wcsicmp(to, changeShortcutTrigger)) {
        ReplaceFixture(changeShortcutPath); changeShortcutPath = nullptr;
    }
    if (moved && moveCalls == exhaustMemoryAfterMove) memory_fault::remaining = 0;
    return moved;
}
#define MoveFileExW TestMove
#define RemoveUnchangedFile TestRemoveUnchangedFile
#define CheckPlainPath TestCheckPlainPath
#define SHChangeNotify TestNotify
#include "../src/SetupCore.cpp"
#undef SHChangeNotify
#undef CheckPlainPath
#undef MoveFileExW
#undef RemoveUnchangedFile
#include "../src/SetupResources.h"
#include "../src/AppVersion.h"
#include "../src/CodexEnv.h"

#define REQUIRE(condition) do { if (!(condition)) { printf("FAIL at line %d: %s\n", __LINE__, #condition); return 20; } } while (0)
// Compare observable arguments using Windows' parser, not private tokenizer structures.
static bool DecodeArguments(const std::wstring& text, std::vector<std::wstring>& values) {
    int count = 0;
    struct Arguments {
        LPWSTR* value;
        ~Arguments() { if (value) LocalFree(value); }
    } args{CommandLineToArgvW((L"program " + text).c_str(), &count)};
    if (!args.value || count < 1) return false;
    values.assign(args.value + 1, args.value + count);
    return true;
}
static int DesktopNotificationTests(const std::wstring& root, setup::Payload payload) {
    using namespace setup;
    const auto base = Join(root, L"desktop-notifications");
    const auto desktop = Join(base, L"redirected\\桌面 Desktop");
    Paths paths{base, Join(base, L"startup.lnk"), Join(base, L"settings.lnk"), Join(desktop, LinkName), {}};
    Options options; options.directory = base; options.startup = options.proxy = options.codexEnv = false;
    options.desktop = true;
    std::wstring target;
    shellNoticeCount = 0;
    REQUIRE(Preflight(paths, options, payload) && shellNoticeCount == 0);
    REQUIRE(!Exists(paths.desktopLink));

    // A failed final rename has no desktop change to publish.
    moveCalls = 0; failMoveAt = 3;
    const auto failed = Apply(paths, options, payload, target);
    failMoveAt = 0;
    REQUIRE(!failed && failed.commit == CommitState::RolledBack && shellNoticeCount == 0);
    REQUIRE(!Exists(paths.desktopLink));

    REQUIRE(Apply(paths, options, payload, target));
    REQUIRE(shellNoticeCount == 2 && NoticeIs(0, SHCNE_CREATE, paths.desktopLink) && NoticeIs(1, SHCNE_UPDATEDIR, desktop));
    REQUIRE(shellNotices[0].attributes != INVALID_FILE_ATTRIBUTES);
    REQUIRE(!(shellNotices[0].attributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_DIRECTORY)));
    Link link;
    REQUIRE(ReadLink(paths.desktopLink, link) && link.target == target && link.description == Marker);

    shellNoticeCount = 0;
    Loaded loaded;
    REQUIRE(Load(paths, loaded) && Preflight(paths, options, payload) && shellNoticeCount == 0);
    options.extra = L"--title=\"中文 空格\"";
    REQUIRE(Apply(paths, options, payload, target));
    REQUIRE(shellNoticeCount == 2 && NoticeIs(0, SHCNE_UPDATEITEM, paths.desktopLink) && NoticeIs(1, SHCNE_UPDATEDIR, desktop));
    std::wstring arguments;
    REQUIRE(BuildArguments(options, arguments) && ReadLink(paths.desktopLink, link) && link.arguments == arguments);

    shellNoticeCount = 0;
    options.desktop = false;
    REQUIRE(Apply(paths, options, payload, target));
    REQUIRE(shellNoticeCount == 2 && NoticeIs(0, SHCNE_DELETE, paths.desktopLink) && NoticeIs(1, SHCNE_UPDATEDIR, desktop));
    REQUIRE(shellNotices[0].attributes == INVALID_FILE_ATTRIBUTES && !Exists(paths.desktopLink));
    shellNoticeCount = 0;
    REQUIRE(Apply(paths, options, payload, target) && shellNoticeCount == 0);
    puts("PASS desktop Shell notifications: final committed paths, create/update/delete, redirected Unicode folders, visible attributes and read-only/no-change silence");
    return 0;
}
static int ShortcutMigrationTests(const std::wstring& root, setup::Payload payload) {
    using namespace setup;
    REQUIRE(std::wstring(LinkName) == L"ChatGPT Launcher.lnk");
    for (int scenario = 0; scenario <= 10; ++scenario) {
        const auto base = Join(root, L"rename-" + std::to_wstring(scenario));
        Paths paths{base, Join(Join(base, L"Startup"), LinkName), Join(base, L"settings.lnk"),
            Join(Join(base, L"桌面 Desktop"), LinkName), {}};
        paths.legacyStartupLink = Join(Parent(paths.startupLink), LegacyLinkName);
        paths.legacyDesktopLink = Join(Parent(paths.desktopLink), LegacyLinkName);
        Paths old = paths; old.startupLink = paths.legacyStartupLink; old.desktopLink = paths.legacyDesktopLink;
        old.legacyStartupLink.clear(); old.legacyDesktopLink.clear();
        Options options; options.directory = base; options.startup = options.desktop = options.proxy = true;
        options.codexEnv = false; options.port = L"12345"; options.extra = L"--title=\"中文 空格\" \"\"";
        std::wstring target, arguments;
        REQUIRE(Apply(old, options, payload, target) && BuildArguments(options, arguments));
        Loaded loaded;
        REQUIRE(Load(paths, loaded) && loaded.options.startup && loaded.options.desktop && loaded.options.port == options.port);
        std::vector<std::wstring> restoredArgs, originalArgs;
        REQUIRE(DecodeArguments(loaded.options.extra, restoredArgs) && DecodeArguments(options.extra, originalArgs) && restoredArgs == originalArgs);
        Snapshot startup, desktop, settings, current;
        REQUIRE(ReadFileBytes(old.startupLink, startup) && ReadFileBytes(old.desktopLink, desktop)
            && ReadFileBytes(old.settingsLink, settings));
        shellNoticeCount = 0;
        REQUIRE(Preflight(paths, options, payload) && shellNoticeCount == 0);
        REQUIRE(!Exists(paths.startupLink) && !Exists(paths.desktopLink) && Exists(old.startupLink) && Exists(old.desktopLink));
        if (scenario == 1) options.startup = options.desktop = false;
        if (scenario == 2) {
            REQUIRE(WriteLink(paths.startupLink, {target, L"", base, L"foreign"}));
            const auto conflict = Apply(paths, options, payload, target);
            REQUIRE(!conflict && conflict.conflictDirectory == Parent(paths.startupLink));
            REQUIRE(ReadFileBytes(old.startupLink, current) && current.data == startup.data);
            continue;
        }
        if (scenario == 3 || scenario == 10) {
            const Link other{Join(base, L"Other.exe"), L"", base, scenario == 3 ? L"foreign" : Marker};
            REQUIRE(WriteLink(old.startupLink, other) && WriteLink(old.desktopLink, other));
            REQUIRE(ReadFileBytes(old.startupLink, startup) && ReadFileBytes(old.desktopLink, desktop));
        }
        if (scenario == 4 || scenario == 7) failDeletePath = old.desktopLink.c_str();
        if (scenario == 7) replaceOnDeleteFailure = old.startupLink.c_str();
        if (scenario == 5) { options.directory = Join(base, L"new-target"); moveCalls = 0; failMoveAt = 5; }
        if (scenario == 6) { changeShortcutPath = old.startupLink.c_str(); changeShortcutTrigger = paths.desktopLink.c_str(); }
        if (scenario == 8) {
            redirectedPath = old.startupLink.c_str();
            REQUIRE(!Preflight(paths, options, payload)); redirectedPath = nullptr;
            REQUIRE(ReadFileBytes(old.startupLink, current) && current.data == startup.data);
            continue;
        }
        if (scenario == 9) {
            const std::wstring preferred = L"--proxy-server=http://127.0.0.1:12346";
            REQUIRE(WriteLink(paths.startupLink, {target, preferred, base, Marker}));
            REQUIRE(Load(paths, loaded) && loaded.options.port == L"12346");
            options = loaded.options; options.codexEnv = false;
        }
        shellNoticeCount = 0;
        const auto saved = Apply(paths, options, payload, target);
        failDeletePath = replaceOnDeleteFailure = changeShortcutPath = changeShortcutTrigger = nullptr; failMoveAt = 0;
        if (scenario >= 4 && scenario <= 7) {
            REQUIRE(!saved && saved.commit == (scenario == 7 ? CommitState::Incomplete : CommitState::RolledBack));
            REQUIRE(shellNoticeCount == 1 && NoticeIs(0, SHCNE_UPDATEDIR, Parent(paths.desktopLink)));
            REQUIRE(ReadFileBytes(old.startupLink, current) && current.exists);
            REQUIRE((scenario == 6 || scenario == 7) ? current.data == std::vector<BYTE>({'f','o','r','e','i','g','n'}) : current.data == startup.data);
            REQUIRE(ReadFileBytes(old.desktopLink, current) && current.data == desktop.data);
            REQUIRE(ReadFileBytes(old.settingsLink, current) && current.data == settings.data);
            REQUIRE(!Exists(paths.startupLink) && !Exists(paths.desktopLink));
        } else {
            REQUIRE(saved && Load(paths, loaded) && loaded.options.startup == options.startup && loaded.options.desktop == options.desktop);
            if (scenario == 1) {
                REQUIRE(shellNoticeCount == 2 && NoticeIs(0, SHCNE_DELETE, old.desktopLink));
            } else if (scenario == 3 || scenario == 10) {
                REQUIRE(shellNoticeCount == 2 && NoticeIs(0, SHCNE_CREATE, paths.desktopLink));
            } else {
                REQUIRE(shellNoticeCount == 3 && NoticeIs(0, SHCNE_CREATE, paths.desktopLink) && NoticeIs(1, SHCNE_DELETE, old.desktopLink));
            }
            REQUIRE(NoticeIs(shellNoticeCount - 1, SHCNE_UPDATEDIR, Parent(paths.desktopLink)));
            if (scenario == 3 || scenario == 10) {
                REQUIRE(ReadFileBytes(old.startupLink, current) && current.data == startup.data);
                REQUIRE(ReadFileBytes(old.desktopLink, current) && current.data == desktop.data);
            } else REQUIRE(!Exists(old.startupLink) && !Exists(old.desktopLink));
            if (options.startup) { Link link; REQUIRE(ReadLink(paths.startupLink, link) && link.target == target && link.description == Marker); }
        }
    }
    puts("PASS shortcut rename: legacy choices/arguments, current-name priority, opt-out, ownership, conflicts, concurrent edits and complete/incomplete rollback");
    return 0;
}
static int ShortcutRemovalTests(const std::wstring& root, setup::Payload payload) {
    using namespace setup;
    const std::vector<BYTE> foreign{'f','o','r','e','i','g','n'};
    for (unsigned scenario = 0; scenario != 4; ++scenario) {
        const auto base = Join(root, L"remove-" + std::to_wstring(scenario));
        Paths paths{base, Join(base, L"startup.lnk"), Join(base, L"settings.lnk"), Join(base, L"desktop.lnk"), {}};
        Options options; options.directory = base; options.startup = options.desktop = true;
        options.proxy = options.codexEnv = false;
        std::wstring target;
        REQUIRE(Apply(paths, options, payload, target));
        Snapshot startup, desktop, settings, current;
        REQUIRE(ReadFileBytes(paths.startupLink, startup) && ReadFileBytes(paths.desktopLink, desktop)
            && ReadFileBytes(paths.settingsLink, settings));
        options.startup = options.desktop = false;
        const auto& changed = scenario == 0 || scenario == 2 ? paths.startupLink : paths.desktopLink;
        if (scenario < 2) {
            changeShortcutPath = changed.c_str(); changeShortcutTrigger = paths.settingsLink.c_str();
        } else {
            if (scenario == 3) {
                paths.legacyDesktopLink = Join(base, L"legacy-desktop.lnk");
                REQUIRE(WriteLink(paths.legacyDesktopLink, {target, L"", base, Marker}));
            }
            failDeletePath = scenario == 2 ? paths.desktopLink.c_str() : paths.legacyDesktopLink.c_str();
            replaceOnDeleteFailure = changed.c_str();
        }
        const auto saved = Apply(paths, options, payload, target);
        failDeletePath = replaceOnDeleteFailure = changeShortcutPath = changeShortcutTrigger = nullptr;
        REQUIRE(!saved && saved.commit == (scenario < 2 ? CommitState::RolledBack : CommitState::Incomplete));
        REQUIRE(saved.code == static_cast<DWORD>(scenario < 2 ? ERROR_RETRY : ERROR_ACCESS_DENIED));
        REQUIRE(ReadFileBytes(changed, current) && current.exists && current.data == foreign);
        const bool changedStartup = scenario == 0 || scenario == 2;
        REQUIRE(ReadFileBytes(changedStartup ? paths.desktopLink : paths.startupLink, current)
            && current.data == (changedStartup ? desktop.data : startup.data));
        REQUIRE(ReadFileBytes(paths.settingsLink, current) && current.data == settings.data);
    }
    puts("PASS shortcut removal: concurrent edits stop removal; rollback restores removed links without overwriting new files");
    return 0;
}
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
    const auto beforeFailure = merged;
    REQUIRE(!MergeCodexEnv({0xff, 0xfe, 0, 0}, options, merged));
    REQUIRE(merged == beforeFailure);
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
    std::vector<std::wstring> originalTokens, restoredTokens;
    REQUIRE(DecodeArguments(args, originalTokens));
    REQUIRE(BuildArguments(parsed, args));
    REQUIRE(DecodeArguments(args, restoredTokens));
    REQUIRE(originalTokens == restoredTokens);
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
        std::vector<std::wstring> tokens;
        REQUIRE(DecodeArguments(QuoteArgument(sample), tokens));
        REQUIRE(tokens.size() == 1 && tokens[0] == sample);
        Options original, roundTrip;
        original.extra = L"--note=" + QuoteArgument(sample) + L"  \"\"";
        std::wstring encoded, restored;
        REQUIRE(BuildArguments(original, encoded) && ParseArguments(encoded, roundTrip));
        REQUIRE(BuildArguments(roundTrip, restored) && restored == encoded);
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
    REQUIRE(DesktopNotificationTests(root, payload) == 0);
    REQUIRE(ShortcutRemovalTests(root, payload) == 0);
    REQUIRE(ShortcutMigrationTests(root, payload) == 0);
    {
        const std::string expected = APP_VERSION_TEXT;
        const std::wstring current(expected.begin(), expected.end());
        REQUIRE(automation::ReadAppVersion(launcherArg) == current && automation::ReadAppVersion(setupArg) == current);
        const std::wstring base = Join(root, L"version 中文");
        REQUIRE(EnsureDirectory(base));
        Paths versionPaths{base, Join(base, L"startup.lnk"), Join(base, L"settings.lnk"), {}, {}};
        const auto file = Join(base, LauncherName);
        REQUIRE(WriteAtomic(file, payloadBytes.data));
        Loaded installed;
        REQUIRE(Load(versionPaths, installed) && !installed.deployed && installed.deployedVersion.empty());
        REQUIRE(WriteLink(versionPaths.settingsLink, {file, L"", base, Marker}));
        REQUIRE(Load(versionPaths, installed) && installed.deployed && installed.deployedVersion == L"" APP_RELEASE_VERSION);
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
        REQUIRE(Load(versionPaths, installed) && installed.deployedVersion == L"" APP_RELEASE_VERSION);
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
        Join(Join(root, L"桌面 Desktop"), LinkName), Join(root, L"profile\\.codex\\.env") };
    Loaded state;
    REQUIRE(Load(paths, state) && !state.hasSettings && state.options.startup && state.options.silent);
    REQUIRE(!state.options.desktop);
    Options options; options.directory = Join(root, L"部署 目录");
    options.startup = true; options.silent = true; options.proxy = true; options.port = L"7897";
    options.desktop = true;
    options.extra = L"--title=\"中文 空格\" \"\"";
    std::wstring target, arguments;
    {
        const auto exe = Join(options.directory, LauncherName);
        redirectedPath = exe.c_str(); moveCalls = 0;
        const auto rejected = Preflight(paths, options, payload);
        redirectedPath = nullptr;
        REQUIRE(!rejected && rejected.code == ERROR_BAD_PATHNAME && moveCalls == 0);
    }
    REQUIRE(Apply(paths, options, payload, target));
    {
        Options removal = options; removal.desktop = false;
        redirectedPath = paths.desktopLink.c_str(); moveCalls = 0;
        const auto rejected = Preflight(paths, removal, payload);
        redirectedPath = nullptr;
        REQUIRE(!rejected && rejected.code == ERROR_BAD_PATHNAME && moveCalls == 0);
    }
    puts("PASS preflight checks redirected launcher files and desktop shortcuts being removed");
    Snapshot envBefore, envAfter;
    REQUIRE(ReadFileBytes(paths.codexEnvFile, envBefore) && envBefore.exists);
    REQUIRE(std::string(envBefore.data.begin(), envBefore.data.end()).find("all_proxy=http://127.0.0.1:7897") == 0);
    const std::string unrelatedEnv = "# keep\nOTHER=fixture-value\n";
    REQUIRE(WriteAtomic(paths.codexEnvFile, {unrelatedEnv.begin(), unrelatedEnv.end()}));
    REQUIRE(Apply(paths, options, payload, target));
    REQUIRE(ReadFileBytes(paths.codexEnvFile, envAfter));
    const std::string appendedEnv = unrelatedEnv + "all_proxy=http://127.0.0.1:7897\nALL_PROXY=http://127.0.0.1:7897\n"
        "HTTP_PROXY=http://127.0.0.1:7897\nHTTPS_PROXY=http://127.0.0.1:7897\n";
    REQUIRE(std::string(envAfter.data.begin(), envAfter.data.end()) == appendedEnv);
    // An existing file with only one proxy key is updated in place; the other three
    // requested keys are appended without disturbing unrelated values or comments.
    const std::string partialEnv = "# keep\nOTHER=fixture-value\nHTTP_PROXY=http://old:1 # proxy comment\n";
    REQUIRE(WriteAtomic(paths.codexEnvFile, {partialEnv.begin(), partialEnv.end()}));
    REQUIRE(Apply(paths, options, payload, target));
    REQUIRE(ReadFileBytes(paths.codexEnvFile, envAfter));
    const std::string filledEnv = "# keep\nOTHER=fixture-value\nHTTP_PROXY=http://127.0.0.1:7897 # proxy comment\n"
        "all_proxy=http://127.0.0.1:7897\nALL_PROXY=http://127.0.0.1:7897\nHTTPS_PROXY=http://127.0.0.1:7897\n";
    REQUIRE(std::string(envAfter.data.begin(), envAfter.data.end()) == filledEnv);
    REQUIRE(Apply(paths, options, payload, target));
    REQUIRE(ReadFileBytes(paths.codexEnvFile, envAfter));
    REQUIRE(std::string(envAfter.data.begin(), envAfter.data.end()) == filledEnv);
    REQUIRE(WriteAtomic(paths.codexEnvFile, envBefore.data));
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
        const auto envFailure = Apply(paths, changed, payload, target); failMoveAt = 0;
        REQUIRE(!envFailure && envFailure.commit == CommitState::RolledBack);
        REQUIRE(!Exists(Join(changed.directory, LauncherName)));
        REQUIRE(ReadFileBytes(paths.settingsLink, file) && file.data == settings.data);
        REQUIRE(ReadFileBytes(paths.startupLink, file) && file.data == startup.data);
        REQUIRE(ReadFileBytes(paths.desktopLink, file) && file.data == desktop.data);
        REQUIRE(ReadFileBytes(paths.codexEnvFile, file) && file.data == envBefore.data);
        // Discovery is the last commit. Its failure must restore the env and links too.
        Snapshot discoveryBefore; const auto discoveryPath = Join(paths.defaultDirectory, L"discovery.json");
        REQUIRE(ReadFileBytes(discoveryPath, discoveryBefore));
        moveCalls = 0; failMoveAt = 6;
        const auto discoveryFailure = Apply(paths, changed, payload, target); failMoveAt = 0;
        REQUIRE(!discoveryFailure && discoveryFailure.commit == CommitState::RolledBack);
        REQUIRE(ReadFileBytes(discoveryPath, file) && file.data == discoveryBefore.data);
        REQUIRE(ReadFileBytes(paths.codexEnvFile, file) && file.data == envBefore.data);
        REQUIRE(ReadFileBytes(paths.settingsLink, file) && file.data == settings.data);
        REQUIRE(!Exists(Join(changed.directory, LauncherName)));
        // Exceptions after any earlier commit must follow the same rollback path as API errors.
        for (unsigned step = 2; step <= 6; ++step) {
            moveCalls = 0; throwMoveAt = step;
            Result failure; bool escaped = false;
            try { failure = Apply(paths, changed, payload, target); }
            catch (...) { escaped = true; }
            throwMoveAt = 0;
            REQUIRE(!escaped && !failure && failure.commit == CommitState::RolledBack && target.empty());
            REQUIRE(ReadFileBytes(discoveryPath, file) && file.data == discoveryBefore.data);
            REQUIRE(ReadFileBytes(paths.codexEnvFile, file) && file.data == envBefore.data);
            REQUIRE(ReadFileBytes(paths.desktopLink, file) && file.data == desktop.data);
            REQUIRE(ReadFileBytes(paths.startupLink, file) && file.data == startup.data);
            REQUIRE(ReadFileBytes(paths.settingsLink, file) && file.data == settings.data);
            REQUIRE(!Exists(Join(changed.directory, LauncherName)));
        }
        // One failed restore must not skip the remaining files.
        moveCalls = 0; failMoveAt = 6; throwMoveAt = 7;
        const auto restoreFailure = Apply(paths, changed, payload, target);
        failMoveAt = 0; throwMoveAt = 0;
        REQUIRE(!restoreFailure && restoreFailure.commit == CommitState::Incomplete && target.empty());
        REQUIRE(ReadFileBytes(paths.desktopLink, file) && file.data == desktop.data);
        REQUIRE(ReadFileBytes(paths.startupLink, file) && file.data == startup.data);
        REQUIRE(ReadFileBytes(paths.settingsLink, file) && file.data == settings.data);
        REQUIRE(!Exists(Join(changed.directory, LauncherName)));
        REQUIRE(WriteAtomic(paths.codexEnvFile, envBefore.data));
        // Persistent OOM must still remove the new EXE and notify any changed desktop directory.
        for (const unsigned step : {2u, 4u}) {
            moveCalls = 0; exhaustMemoryAfterMove = step; shellNoticeCount = 0;
            Result exhausted; bool escaped = false;
            {
                memory_fault::Scope fault;
                try { exhausted = Apply(paths, changed, payload, target); }
                catch (...) { escaped = true; }
            }
            exhaustMemoryAfterMove = 0;
            REQUIRE(!escaped && !exhausted && exhausted.code == ERROR_NOT_ENOUGH_MEMORY);
            REQUIRE(exhausted.commit == CommitState::Incomplete && !exhausted.message.empty() && target.empty());
            REQUIRE(!Exists(Join(changed.directory, LauncherName)));
            if (step == 4) REQUIRE(shellNoticeCount == 1 && NoticeIs(0, SHCNE_UPDATEDIR, Parent(paths.desktopLink)));
            else {
                REQUIRE(shellNoticeCount == 0);
                REQUIRE(ReadFileBytes(paths.startupLink, file) && file.data == startup.data);
                REQUIRE(ReadFileBytes(paths.desktopLink, file) && file.data == desktop.data);
            }
            REQUIRE(WriteAtomic(paths.settingsLink, settings.data));
            REQUIRE(WriteAtomic(paths.startupLink, startup.data));
            REQUIRE(WriteAtomic(paths.desktopLink, desktop.data));
        }
        puts("PASS deployment exceptions: all commit boundaries, independent rollback and persistent allocation failure");
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
    // A rollback write failure must be distinct from a complete rollback.
    moveCalls = 0; failMoveAt = 2; failSecondMoveAt = 3;
    const auto incomplete = Apply(paths, options, payload, target); failMoveAt = 0; failSecondMoveAt = 0;
    REQUIRE(!incomplete && incomplete.commit == CommitState::Incomplete);
    REQUIRE(WriteAtomic(paths.settingsLink, settingsBefore.data));
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
