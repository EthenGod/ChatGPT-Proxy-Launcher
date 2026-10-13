// SPDX-License-Identifier: GPL-3.0-only
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <appmodel.h>
#include <stdio.h>
#include <string>
#include <vector>

// Compile-time substitutions live only in this test translation unit.
static LONG WINAPI TestPackages(PCWSTR, UINT32*, PWSTR*, UINT32*, PWSTR);
static LONG WINAPI TestPath(PCWSTR, UINT32*, PWSTR);
static DWORD WINAPI TestAttributes(PCWSTR);
static BOOL WINAPI TestCreate(PCWSTR, PWSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES,
    BOOL, DWORD, LPVOID, PCWSTR, LPSTARTUPINFOW, LPPROCESS_INFORMATION);
static DWORD WINAPI TestMessage(DWORD, LPCVOID, DWORD, DWORD, LPWSTR, DWORD, va_list*);
static LPVOID WINAPI TestAllocate(HANDLE, DWORD, SIZE_T);
static BOOL WINAPI TestFree(HANDLE, DWORD, LPVOID);
#define GetPackagesByPackageFamily TestPackages
#define GetPackagePathByFullName TestPath
#define GetFileAttributesW TestAttributes
#define CreateProcessW TestCreate
#define FormatMessageW TestMessage
#define HeapAlloc TestAllocate
#define HeapFree TestFree
#include "../src/ChatGPTProxyLauncher.cpp"
#undef GetPackagesByPackageFamily
#undef GetPackagePathByFullName
#undef GetFileAttributesW
#undef CreateProcessW
#undef FormatMessageW
#undef HeapAlloc
#undef HeapFree

enum class Scenario { Normal, Empty, GrowOnce, GrowAlways, PathGoneOnce, PathGoneAlways,
    MissingExe, Denied, Directory, HugeCount, UnexpectedSuccess, BadIdentity, ResourcesOnly };
static bool fakePackages = false, fakeCreate = false;
static Scenario scenario = Scenario::Normal;
static unsigned enumerations = 0, messageCalls = 0;
static unsigned allocationCalls = 0, failAllocation = 0;
static unsigned liveAllocations = 0, createCalls = 0;
static bool createSuccess = false;
static HANDLE createdProcess = nullptr, createdThread = nullptr;
static std::wstring selected, workingDirectory, application, forwarded;
static const wchar_t Newest[] = L"OpenAI.Codex_10.2.0.0_x64__2p2nqsd0c76g0";
static const wchar_t Updated[] = L"OpenAI.Codex_11.0.0.0_x64__2p2nqsd0c76g0";

static std::vector<std::wstring> Packages() {
    if (scenario == Scenario::BadIdentity) return { L"invalid package identity" };
    if (scenario == Scenario::ResourcesOnly) return { L"OpenAI.Codex_99.0.0.0_x64_en-us_2p2nqsd0c76g0" };
    if (scenario == Scenario::PathGoneOnce && enumerations > 1) return { Updated };
    return { L"OpenAI.Codex_9.99.0.0_x64__2p2nqsd0c76g0", Newest,
        L"OpenAI.Codex_99.0.0.0_x64_en-us_2p2nqsd0c76g0",
        L"OpenAI.Codex_10.1.999.0_x64__2p2nqsd0c76g0",
        L"OpenAI.Other_99.0.0.0_x64__2p2nqsd0c76g0",
        L"OpenAI.Codex_99.0.0.0_x64__aaaaaaaaaaaaa" };
}

static LONG WINAPI TestPackages(PCWSTR family, UINT32* count, PWSTR* names, UINT32* chars, PWSTR buffer) {
    if (!fakePackages) return GetPackagesByPackageFamily(family, count, names, chars, buffer);
    if (!names) ++enumerations;
    if (scenario == Scenario::Empty) { *count = 0; return ERROR_SUCCESS; }
    if (scenario == Scenario::UnexpectedSuccess) { *count = 1; return ERROR_SUCCESS; }
    if (scenario == Scenario::HugeCount) { *count = 1025; *chars = 1; return ERROR_INSUFFICIENT_BUFFER; }
    const auto packages = Packages();
    UINT32 needed = 0;
    for (const auto& name : packages) needed += static_cast<UINT32>(name.size() + 1);
    if (!names || *count < packages.size() || *chars < needed
        || scenario == Scenario::GrowAlways || (scenario == Scenario::GrowOnce && enumerations == 1)) {
        *count = static_cast<UINT32>(packages.size()); *chars = needed;
        return ERROR_INSUFFICIENT_BUFFER;
    }
    for (size_t i = 0; i < packages.size(); ++i) {
        names[i] = buffer;
        CopyMemory(buffer, packages[i].c_str(), (packages[i].size() + 1) * sizeof(wchar_t));
        buffer += packages[i].size() + 1;
    }
    *count = static_cast<UINT32>(packages.size()); *chars = needed;
    return ERROR_SUCCESS;
}

static LONG WINAPI TestPath(PCWSTR name, UINT32* length, PWSTR path) {
    if (!fakePackages) return GetPackagePathByFullName(name, length, path);
    selected = name;
    if (scenario == Scenario::PathGoneAlways || (scenario == Scenario::PathGoneOnce && enumerations == 1))
        return ERROR_NOT_FOUND;
    if (scenario == Scenario::Denied) return ERROR_ACCESS_DENIED;
    const wchar_t root[] = L"C:\\Launcher Test\\Package";
    const UINT32 required = sizeof(root) / sizeof(wchar_t);
    if (*length < required) { *length = required; return ERROR_INSUFFICIENT_BUFFER; }
    CopyMemory(path, root, sizeof(root)); *length = required;
    return ERROR_SUCCESS;
}

static DWORD WINAPI TestAttributes(PCWSTR path) {
    if (!fakePackages) return GetFileAttributesW(path);
    if (scenario == Scenario::MissingExe) { SetLastError(ERROR_FILE_NOT_FOUND); return INVALID_FILE_ATTRIBUTES; }
    return scenario == Scenario::Directory ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
}

static BOOL WINAPI TestCreate(PCWSTR exe, PWSTR command, LPSECURITY_ATTRIBUTES pa, LPSECURITY_ATTRIBUTES ta,
    BOOL inherit, DWORD flags, LPVOID environment, PCWSTR directory, LPSTARTUPINFOW startup, LPPROCESS_INFORMATION process) {
    if (!fakeCreate) return CreateProcessW(exe, command, pa, ta, inherit, flags, environment, directory, startup, process);
    ++createCalls;
    application = exe; forwarded = command; workingDirectory = directory;
    if (inherit || flags || pa || ta || environment || startup->dwFlags) {
        SetLastError(ERROR_INVALID_PARAMETER); return FALSE;
    }
    if (createSuccess) {
        createdProcess = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        createdThread = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!createdProcess || !createdThread) {
            if (createdProcess) CloseHandle(createdProcess);
            if (createdThread) CloseHandle(createdThread);
            SetLastError(ERROR_NOT_ENOUGH_MEMORY); return FALSE;
        }
        process->hProcess = createdProcess; process->hThread = createdThread;
        return TRUE;
    }
    SetLastError(ERROR_ACCESS_DENIED); return FALSE;
}

static DWORD WINAPI TestMessage(DWORD flags, LPCVOID source, DWORD error, DWORD language, LPWSTR buffer,
    DWORD size, va_list* arguments) {
    ++messageCalls;
    return FormatMessageW(flags, source, error, language, buffer, size, arguments);
}

static LPVOID WINAPI TestAllocate(HANDLE heap, DWORD flags, SIZE_T bytes) {
    if (++allocationCalls == failAllocation) return nullptr;
    LPVOID value = HeapAlloc(heap, flags, bytes);
    if (value) ++liveAllocations;
    return value;
}

static BOOL WINAPI TestFree(HANDLE heap, DWORD flags, LPVOID value) {
    const BOOL freed = HeapFree(heap, flags, value);
    if (freed && value) --liveAllocations;
    return freed;
}

#define REQUIRE(condition) do { if (!(condition)) { printf("FAIL at line %d: %s\n", __LINE__, #condition); return 20; } } while (0)

static int CheckLookup() {
    fakePackages = true;
    struct Case { Scenario state; DWORD result; unsigned calls; };
    const Case cases[] = {
        { Scenario::Normal, 0, 1 }, { Scenario::Empty, ERROR_NOT_FOUND, 1 },
        { Scenario::GrowOnce, 0, 2 }, { Scenario::GrowAlways, ERROR_RETRY, 3 },
        { Scenario::PathGoneOnce, 0, 2 }, { Scenario::PathGoneAlways, ERROR_NOT_FOUND, 3 },
        { Scenario::MissingExe, ERROR_FILE_NOT_FOUND, 3 }, { Scenario::Denied, ERROR_ACCESS_DENIED, 1 },
        { Scenario::Directory, ERROR_FILE_NOT_FOUND, 1 }, { Scenario::HugeCount, ERROR_INVALID_DATA, 1 },
        { Scenario::UnexpectedSuccess, ERROR_INVALID_DATA, 1 }, { Scenario::ResourcesOnly, ERROR_NOT_FOUND, 1 }
    };
    wchar_t path[32768];
    for (const auto& item : cases) {
        scenario = item.state; enumerations = 0; selected.clear();
        REQUIRE(FindExecutable(path, 32768) == item.result);
        REQUIRE(liveAllocations == 0);
        REQUIRE(enumerations == item.calls);
        if (!item.result) {
            REQUIRE(selected == (scenario == Scenario::PathGoneOnce ? Updated : Newest));
            REQUIRE(wcscmp(path, L"C:\\Launcher Test\\Package\\app\\ChatGPT.exe") == 0);
        }
        if (scenario == Scenario::MissingExe) REQUIRE(selected == Newest);
    }
    scenario = Scenario::BadIdentity; enumerations = 0;
    REQUIRE(FindExecutable(path, 32768) != 0);
    scenario = Scenario::Normal;
    REQUIRE(FindExecutable(path, 2) == ERROR_INSUFFICIENT_BUFFER);
    REQUIRE(FindExecutable(path, 30) == ERROR_FILENAME_EXCED_RANGE);
    for (unsigned failure = 1; failure <= 8; ++failure) {
        allocationCalls = 0; failAllocation = failure;
        REQUIRE(FindExecutable(path, 32768) == ERROR_NOT_ENOUGH_MEMORY);
        REQUIRE(liveAllocations == 0);
    }
    failAllocation = 0;
    fakePackages = false;
    return 0;
}

static int CheckEntryAndLifetime() {
    fakePackages = true; fakeCreate = true; createSuccess = true; scenario = Scenario::Normal;
    const HANDLE oldOut = GetStdHandle(STD_OUTPUT_HANDLE), oldError = GetStdHandle(STD_ERROR_HANDLE);
    HANDLE sink = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, 0, nullptr);
    REQUIRE(sink != INVALID_HANDLE_VALUE);
    REQUIRE(SetStdHandle(STD_OUTPUT_HANDLE, sink) && SetStdHandle(STD_ERROR_HANDLE, sink));
    struct EntryCase { PCWSTR tail; bool check; };
    const EntryCase cases[] = {
        { L"--launcher-check", true }, { L"--launcher-check\t--value=\"中文 空格\"", true },
        { L"--launcher-check --proxy-server=http://127.0.0.1:7897", true },
        { L"", false }, { L"--launcher-check-extra", false }, { L"--launcher-check=value", false },
        { L"--proxy-server=http://127.0.0.1:7897 --no-startup-window", false }
    };
    for (const auto& item : cases) {
        std::wstring tail = item.tail;
        createCalls = 0; messageCalls = 0;
        REQUIRE(wWinMain(nullptr, nullptr, &tail[0], 0) == 0);
        REQUIRE(createCalls == (item.check ? 0u : 1u));
        REQUIRE(messageCalls == 0 && liveAllocations == 0);
        if (!item.check) {
            REQUIRE(forwarded == std::wstring(L"\"C:\\Launcher Test\\Package\\app\\ChatGPT.exe\"")
                + (tail.empty() ? L"" : L" ") + tail);
            DWORD flags = 0;
            REQUIRE(!GetHandleInformation(createdProcess, &flags) && GetLastError() == ERROR_INVALID_HANDLE);
            REQUIRE(!GetHandleInformation(createdThread, &flags) && GetLastError() == ERROR_INVALID_HANDLE);
        }
    }
    createSuccess = false; createCalls = 0;
    std::wstring launch = L"--test";
    REQUIRE(wWinMain(nullptr, nullptr, &launch[0], 0) == ERROR_ACCESS_DENIED);
    REQUIRE(createCalls == 1 && liveAllocations == 0);
    createSuccess = true;
    std::wstring check = L"--launcher-check";
    allocationCalls = 0; failAllocation = 1; createCalls = 0;
    REQUIRE(wWinMain(nullptr, nullptr, &check[0], 0) == ERROR_NOT_ENOUGH_MEMORY);
    REQUIRE(createCalls == 0 && liveAllocations == 0);
    failAllocation = 0;
    scenario = Scenario::Empty;
    REQUIRE(wWinMain(nullptr, nullptr, &check[0], 0) == ERROR_NOT_FOUND);
    REQUIRE(createCalls == 0 && liveAllocations == 0);
    scenario = Scenario::Normal;
    REQUIRE(SetStdHandle(STD_OUTPUT_HANDLE, oldOut) && SetStdHandle(STD_ERROR_HANDLE, oldError));
    CloseHandle(sink);
    wchar_t path[32768];
    for (unsigned i = 0; i < 1000; ++i) {
        REQUIRE(FindExecutable(path, 32768) == 0);
        REQUIRE(StartApplication(path, L"--test") == 0);
        REQUIRE(liveAllocations == 0);
    }
    fakePackages = false; fakeCreate = false; createSuccess = false;
    return 0;
}

static int CheckRandomCommands(PCWSTR executable) {
    unsigned random = 0x2468u;
    const wchar_t alphabet[] = L"abZ09 \\\"\t;&|<>%中文";
    for (unsigned iteration = 0; iteration < 500; ++iteration) {
        random = random * 1664525u + 1013904223u;
        std::wstring args(random % 4096, L' ');
        for (auto& character : args) {
            random = random * 1664525u + 1013904223u;
            character = alphabet[random % (sizeof(alphabet) / sizeof(wchar_t) - 1)];
        }
        const std::wstring expected = std::wstring(L"\"") + executable + L"\"" + (args.empty() ? L"" : L" ") + args;
        const size_t capacity = expected.size() + 1;
        std::vector<wchar_t> guarded(capacity + 2, L'!');
        REQUIRE(BuildCommand(executable, args.c_str(), guarded.data() + 1, capacity - 1) == ERROR_FILENAME_EXCED_RANGE);
        for (wchar_t value : guarded) REQUIRE(value == L'!');
        REQUIRE(BuildCommand(executable, args.c_str(), guarded.data() + 1, capacity) == 0);
        REQUIRE(guarded.front() == L'!' && guarded.back() == L'!');
        REQUIRE(expected == guarded.data() + 1);
    }
    return 0;
}

// Test-only child process: capture the actual Windows command line.
int wmain(int argc, wchar_t** argv) {
    if (argc >= 3 && wcscmp(argv[1], L"--capture") == 0) {
        HANDLE file = CreateFileW(argv[2], GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return 1;
        const wchar_t* line = GetCommandLineW();
        DWORD written = 0;
        WriteFile(file, line, static_cast<DWORD>(wcslen(line) * sizeof(wchar_t)), &written, nullptr);
        CloseHandle(file);
        return 0;
    }
    if (argc != 2) return 2;
    REQUIRE(CheckLookup() == 0);
    REQUIRE(CheckEntryAndLifetime() == 0);
    messageCalls = 0;
    REQUIRE(ReportError(0) == 0 && messageCalls == 0);
    wchar_t executable[32768], command[32768];
    if (!GetModuleFileNameW(nullptr, executable, 32768)) return 3;
    REQUIRE(CheckRandomCommands(executable) == 0);
    const wchar_t* cases[] = {
        L"", L"--proxy-server=http://127.0.0.1:7897 --no-startup-window",
        L"--proxy-server=\"socks5://127.0.0.1:1080\" --proxy-bypass-list=\"localhost;*.example.com\"",
        L"--example=\"C:\\some folder\\中文\\\"\"  --value=\"quoted \\\"text\\\"\"\t--flag",
        L"--startup --check --plan --stdout --launcher-check-extra",
        L"--value=\"中文 空格\" --literal=\"& | > %PATH%\""
    };
    for (const auto tail : cases) {
        const std::wstring expected = std::wstring(L"\"") + executable + L"\"" + (*tail ? L" " : L"") + tail;
        if (BuildCommand(executable, tail, command, 32768) || expected != command) return 4;
    }
    std::wstring oversized(32767, L'x');
    if (BuildCommand(executable, oversized.c_str(), command, 32768) != ERROR_FILENAME_EXCED_RANGE) return 5;
    if (BuildCommand(executable, L"", command, 2) != ERROR_FILENAME_EXCED_RANGE) return 6;
    const std::wstring boundary(32767 - wcslen(executable) - 4, L'x');
    REQUIRE(BuildCommand(executable, boundary.c_str(), command, 32767) == 0);
    REQUIRE(wcslen(command) == 32766);
    REQUIRE(BuildCommand(executable, (boundary + L"x").c_str(), command, 32768) == ERROR_FILENAME_EXCED_RANGE);
    REQUIRE(BuildCommand(executable, L"", command, wcslen(executable) + 3) == 0);
    REQUIRE(StartApplication(L"missing.exe", L"") == ERROR_BAD_PATHNAME);
    fakeCreate = true;
    for (unsigned failure = 1; failure <= 2; ++failure) {
        allocationCalls = 0; failAllocation = failure;
        REQUIRE(StartApplication(L"C:\\child.exe", L"") == ERROR_NOT_ENOUGH_MEMORY);
    }
    failAllocation = 0;
    REQUIRE(StartApplication(L"C:\\child.exe", L"--value=\"中文 空格\"") == ERROR_ACCESS_DENIED);
    REQUIRE(workingDirectory == L"C:\\" && application == L"C:\\child.exe");
    REQUIRE(forwarded == L"\"C:\\child.exe\" --value=\"中文 空格\"");
    REQUIRE(StartApplication(L"C:\\test folder\\child.exe", L"") == ERROR_ACCESS_DENIED);
    REQUIRE(workingDirectory == L"C:\\test folder");
    fakeCreate = false;
    DWORD missing = StartApplication(L"C:\\nonexistent-launcher-test-123\\missing.exe", L"");
    if (missing != ERROR_FILE_NOT_FOUND && missing != ERROR_PATH_NOT_FOUND && missing != ERROR_DIRECTORY) {
        printf("Unexpected error: %lu\n", missing);
        return 7;
    }

    // Quotes, spaces and Unicode in the path are received by an actual child, not a shell.
    unsigned index = 0;
    for (const auto tail : cases) {
        const std::wstring capturePath = std::wstring(argv[1]) + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(index++);
        const std::wstring args = std::wstring(L"--capture \"") + capturePath + L"\" " + tail;
        const DWORD result = StartApplication(executable, args.c_str());
        if (result) return static_cast<int>(result);
        HANDLE capture = INVALID_HANDLE_VALUE;
        for (int retry = 0; retry < 100; ++retry) {
            capture = CreateFileW(capturePath.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (capture != INVALID_HANDLE_VALUE) break;
            Sleep(20);
        }
        if (capture == INVALID_HANDLE_VALUE) return 8;
        DWORD bytes = 0;
        ZeroMemory(command, sizeof(command));
        const BOOL read = ReadFile(capture, command, sizeof(command) - sizeof(wchar_t), &bytes, nullptr);
        CloseHandle(capture);
        const std::wstring expected = std::wstring(L"\"") + executable + L"\" " + args;
        if (!read || expected != command) return 9;
    }
    REQUIRE(liveAllocations == 0);
    puts("PASS: package selection, retries, entry dispatch, allocation/handle lifetime, 500 randomized commands, boundaries and real child forwarding.");
    return 0;
}
