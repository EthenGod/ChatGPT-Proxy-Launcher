// SPDX-License-Identifier: GPL-3.0-only
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <objbase.h>
#include <appmodel.h>
#include <stdio.h>
#include <string>
#include <vector>
#include "../src/TaskbarShortcut.h"
#include "../src/StartupApps.h"
#include "../src/CompanionRunner.h"
#include "MemoryFault.h"
static unsigned companionCalls = 0;
static unsigned companionFailure = 0;
void companions::Run(PCWSTR, Start) noexcept { ++companionCalls; }
automation::Json chain::Run(const RunOptions&, PCWSTR, companions::Start) {
    if (companionFailure) {
        if (companionFailure == 2) memory_fault::remaining = 0;
        throw std::bad_alloc();
    }
    ++companionCalls; auto result = automation::Json::Object(); result[L"entries"] = automation::Json::Array();
    result[L"chainStatus"] = automation::Json(L"completed"); result[L"exitCode"] = automation::Json::Number(0); return result;
}
std::wstring chain::RootDirectory() { return L"C:\\isolated-chain-tests"; }
static unsigned repairCalls = 0;
void RepairTaskbarShortcut(PCWSTR) noexcept { ++repairCalls; }

// Compile-time substitutions live only in this test translation unit.
static LONG WINAPI TestPackages(PCWSTR, UINT32*, PWSTR*, UINT32*, PWSTR);
static LONG WINAPI TestIdentity(PCWSTR, UINT32, UINT32*, BYTE*);
static LONG WINAPI TestPath(PCWSTR, UINT32*, PWSTR);
static DWORD WINAPI TestAttributes(PCWSTR);
static BOOL WINAPI TestCreate(PCWSTR, PWSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES,
    BOOL, DWORD, LPVOID, PCWSTR, LPSTARTUPINFOW, LPPROCESS_INFORMATION);
static DWORD WINAPI TestMessage(DWORD, LPCVOID, DWORD, DWORD, LPWSTR, DWORD, va_list*);
static LPVOID WINAPI TestAllocate(HANDLE, DWORD, SIZE_T);
static BOOL WINAPI TestFree(HANDLE, DWORD, LPVOID);
#define GetPackagesByPackageFamily TestPackages
#define PackageIdFromFullName TestIdentity
#define GetPackagePathByFullName TestPath
#define GetFileAttributesW TestAttributes
#define CreateProcessW TestCreate
#define FormatMessageW TestMessage
#define HeapAlloc TestAllocate
#define HeapFree TestFree
#include "../src/ChatGPTProxyLauncher.cpp"
#undef GetPackagesByPackageFamily
#undef PackageIdFromFullName
#undef GetPackagePathByFullName
#undef GetFileAttributesW
#undef CreateProcessW
#undef FormatMessageW
#undef HeapAlloc
#undef HeapFree

enum class Scenario { Normal, Empty, GrowOnce, GrowAlways, PathGoneOnce, PathGoneAlways,
    MissingExe, Denied, Directory, HugeCount, UnexpectedSuccess, BadIdentity, ResourcesOnly,
    Single, LargeList, LargeIdentity, HugeIdentity };
static bool fakePackages = false, fakeCreate = false;
static Scenario scenario = Scenario::Normal;
static unsigned messageCalls = 0;
static unsigned packageCalls = 0, identityCalls = 0;
static unsigned allocationCalls = 0, failAllocation = 0;
static unsigned liveAllocations = 0, createCalls = 0;
static bool createSuccess = false;
static HANDLE createdProcess = nullptr, createdThread = nullptr;
static std::wstring selected, workingDirectory, application, forwarded;
static const wchar_t Newest[] = L"OpenAI.Codex_10.2.0.0_x64__2p2nqsd0c76g0";
static const wchar_t Updated[] = L"OpenAI.Codex_11.0.0.0_x64__2p2nqsd0c76g0";

static const std::vector<std::wstring>& Packages() {
    static const std::vector<std::wstring> invalid{ L"invalid package identity" };
    static const std::vector<std::wstring> resources{ L"OpenAI.Codex_99.0.0.0_x64_en-us_2p2nqsd0c76g0" };
    static const std::vector<std::wstring> updated{ Updated }, single{ Newest };
    static const std::vector<std::wstring> multiple{ L"OpenAI.Codex_9.99.0.0_x64__2p2nqsd0c76g0", Newest,
        L"OpenAI.Codex_99.0.0.0_x64_en-us_2p2nqsd0c76g0",
        L"OpenAI.Codex_10.1.999.0_x64__2p2nqsd0c76g0",
        L"OpenAI.Other_99.0.0.0_x64__2p2nqsd0c76g0",
        L"OpenAI.Codex_99.0.0.0_x64__aaaaaaaaaaaaa" };
    static const auto large = [] {
        auto catalog = multiple;
        for (int i = 0; i < 4; ++i)
            catalog.push_back(L"OpenAI.Codex_3." + std::to_wstring(i) + L".0.0_x64__2p2nqsd0c76g0");
        return catalog;
    }();
    if (scenario == Scenario::BadIdentity) return invalid;
    if (scenario == Scenario::ResourcesOnly) return resources;
    if (scenario == Scenario::PathGoneOnce && packageCalls > 1) return updated;
    if (scenario == Scenario::Single) return single;
    if (scenario == Scenario::LargeList || scenario == Scenario::GrowOnce || scenario == Scenario::GrowAlways)
        return large;
    return multiple;
}

static LONG WINAPI TestPackages(PCWSTR family, UINT32* count, PWSTR* names, UINT32* chars, PWSTR buffer) {
    if (!fakePackages) return GetPackagesByPackageFamily(family, count, names, chars, buffer);
    ++packageCalls;
    if (scenario == Scenario::Empty) { *count = 0; return ERROR_SUCCESS; }
    if (scenario == Scenario::UnexpectedSuccess) { *count = 1; return ERROR_SUCCESS; }
    if (scenario == Scenario::HugeCount) { *count = 1025; *chars = 1; return ERROR_INSUFFICIENT_BUFFER; }
    const auto& packages = Packages();
    UINT32 needed = 0;
    for (const auto& name : packages) needed += static_cast<UINT32>(name.size() + 1);
    // Store updates can add a package after a size report but before the retry.
    const bool changed = scenario == Scenario::GrowAlways || (scenario == Scenario::GrowOnce && packageCalls == 2);
    if (!names || *count < packages.size() || *chars < needed || changed) {
        *count = static_cast<UINT32>(packages.size()) + (changed ? 1 : 0); *chars = needed + (changed ? 64 : 0);
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

static LONG WINAPI TestIdentity(PCWSTR name, UINT32 flags, UINT32* length, BYTE* buffer) {
    ++identityCalls;
    if (scenario == Scenario::HugeIdentity) { *length = 131073; return ERROR_INSUFFICIENT_BUFFER; }
    if (scenario == Scenario::LargeIdentity && *length < 1024) {
        *length = 1024;
        return ERROR_INSUFFICIENT_BUFFER;
    }
    return PackageIdFromFullName(name, flags, length, buffer);
}

static LONG WINAPI TestPath(PCWSTR name, UINT32* length, PWSTR path) {
    if (!fakePackages) return GetPackagePathByFullName(name, length, path);
    selected = name;
    if (scenario == Scenario::PathGoneAlways || (scenario == Scenario::PathGoneOnce && packageCalls == 1))
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
        { Scenario::LargeList, 0, 2 }, { Scenario::LargeIdentity, 0, 1 },
        { Scenario::HugeIdentity, ERROR_INVALID_DATA, 1 },
        { Scenario::GrowOnce, 0, 4 }, { Scenario::GrowAlways, ERROR_RETRY, 6 },
        { Scenario::PathGoneOnce, 0, 2 }, { Scenario::PathGoneAlways, ERROR_NOT_FOUND, 3 },
        { Scenario::MissingExe, ERROR_FILE_NOT_FOUND, 3 }, { Scenario::Denied, ERROR_ACCESS_DENIED, 1 },
        { Scenario::Directory, ERROR_FILE_NOT_FOUND, 1 }, { Scenario::HugeCount, ERROR_INVALID_DATA, 1 },
        { Scenario::UnexpectedSuccess, ERROR_INVALID_DATA, 1 }, { Scenario::ResourcesOnly, ERROR_NOT_FOUND, 1 }
    };
    wchar_t path[32768];
    for (const auto& item : cases) {
        scenario = item.state; packageCalls = identityCalls = allocationCalls = 0; selected.clear();
        REQUIRE(FindExecutable(path, 32768) == item.result);
        REQUIRE(liveAllocations == 0);
        REQUIRE(packageCalls == item.calls);
        if (scenario == Scenario::Normal) REQUIRE(identityCalls == Packages().size() && allocationCalls == 0);
        if (!item.result) {
            REQUIRE(selected == (scenario == Scenario::PathGoneOnce ? Updated : Newest));
            REQUIRE(wcscmp(path, L"C:\\Launcher Test\\Package\\app\\ChatGPT.exe") == 0);
        }
        if (scenario == Scenario::MissingExe) REQUIRE(selected == Newest);
    }
    scenario = Scenario::BadIdentity;
    REQUIRE(FindExecutable(path, 32768) != 0);
    scenario = Scenario::Normal;
    REQUIRE(FindExecutable(path, 2) == ERROR_INSUFFICIENT_BUFFER);
    REQUIRE(FindExecutable(path, 30) == ERROR_FILENAME_EXCED_RANGE);
    for (auto overflow : { Scenario::LargeList, Scenario::LargeIdentity }) {
        scenario = overflow;
        failAllocation = allocationCalls = 0;
        REQUIRE(FindExecutable(path, _countof(path)) == 0 && allocationCalls > 0);
        const auto allocations = allocationCalls;
        for (unsigned failure = 1; failure <= allocations; ++failure) {
            allocationCalls = 0; failAllocation = failure;
            REQUIRE(FindExecutable(path, _countof(path)) == ERROR_NOT_ENOUGH_MEMORY);
            REQUIRE(liveAllocations == 0);
        }
    }
    failAllocation = 0;
    fakePackages = false;
    return 0;
}

static int CheckEntryAndLifetime(const std::wstring& outputDir) {
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
        { L"\"--launcher-check\"", true }, { L" \t\"--launcher-check\"  \"\" \"中文 空格\"", true },
        { L"--\"launcher\"-check\t--flag=\"a b\"", true },
        { L"", false }, { L"--launcher-check-extra", false }, { L"--launcher-check=value", false },
        { L" \"--launcher-check-extra\" \"\"", false }, { L"\"--launcher-check value\"", false },
        { L"\"\" --launcher-check", false },
        { L"--proxy-server=http://127.0.0.1:7897 --no-startup-window", false }
    };
    for (const auto& item : cases) {
        std::wstring tail = item.tail;
        createCalls = 0; messageCalls = 0; repairCalls = 0; companionCalls = 0;
        REQUIRE(wWinMain(nullptr, nullptr, &tail[0], 0) == 0);
        REQUIRE(createCalls == (item.check ? 0u : 1u));
        REQUIRE(repairCalls == (item.check ? 0u : 1u));
        REQUIRE(companionCalls == (item.check ? 0u : 1u));
        REQUIRE(messageCalls == 0 && liveAllocations == 0);
        if (!item.check) {
            REQUIRE(forwarded == std::wstring(L"\"C:\\Launcher Test\\Package\\app\\ChatGPT.exe\"")
                + (tail.empty() ? L"" : L" ") + tail);
            DWORD flags = 0;
            REQUIRE(!GetHandleInformation(createdProcess, &flags) && GetLastError() == ERROR_INVALID_HANDLE);
            REQUIRE(!GetHandleInformation(createdThread, &flags) && GetLastError() == ERROR_INVALID_HANDLE);
        }
    }
    chain::Invocation diagnostic;
    REQUIRE(diagnostic.Parse(L" \t\"--launcher-check\"\t \"\"  --value=\"中文 空格\"  ") == 0);
    REQUIRE(diagnostic.mode == chain::Invocation::Mode::Check
        && std::wstring_view(diagnostic.forwarded) == L"\"\"  --value=\"中文 空格\"  ");
    createSuccess = false; createCalls = 0; companionCalls = 0;
    std::wstring launch = L"--test";
    REQUIRE(wWinMain(nullptr, nullptr, &launch[0], 0) == ERROR_ACCESS_DENIED);
    REQUIRE(!companionCalls);
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
    for (unsigned failure = 1; failure <= 2; ++failure) {
        companionFailure = failure;
        const int chainError = failure == 2 ? 70 : 10;
        const auto output = outputDir + L"\\failure-" + std::to_wstring(failure) + L".json";
        for (unsigned explicitChain = 0; explicitChain <= 1; ++explicitChain) {
            std::wstring tail = explicitChain ? L"--launcher-chain --result \"" + output + L"\" --feedback quiet --" : L"--test";
            int code = -1;
            { memory_fault::Scope fault; code = wWinMain(nullptr, nullptr, tail.data(), 0); }
            const int expected = explicitChain ? chainError : 0;
            if (code != expected) printf("Failure case %u, explicit chain %u: expected %d, got %d\n", failure, explicitChain, expected, code);
            REQUIRE(code == expected);
            REQUIRE(liveAllocations == 0);
        }
    }
    companionFailure = 0;
    puts("PASS launcher allocation failure boundary: ordinary launch retains success; explicit mode returns 10 or 70 without termination");
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
    if (argc == 2 && wcscmp(argv[1], L"--benchmark-lookup") == 0) {
        fakePackages = true;
        wchar_t path[32768];
        LARGE_INTEGER frequency, begin, end;
        REQUIRE(QueryPerformanceFrequency(&frequency));
        constexpr unsigned iterations = 20000;
        for (auto catalog : { Scenario::Single, Scenario::Normal }) {
            scenario = catalog;
            for (int warmup = 0; warmup < 100; ++warmup) REQUIRE(FindExecutable(path, _countof(path)) == 0);
            packageCalls = identityCalls = allocationCalls = 0;
            QueryPerformanceCounter(&begin);
            for (unsigned i = 0; i < iterations; ++i) REQUIRE(FindExecutable(path, _countof(path)) == 0);
            QueryPerformanceCounter(&end);
            REQUIRE(selected == Newest && liveAllocations == 0);
            printf("packages=%zu lookup_us=%.3f enumeration=%u identity=%u allocations=%u\n", Packages().size(),
                1e6 * (end.QuadPart - begin.QuadPart) / frequency.QuadPart / iterations,
                packageCalls / iterations, identityCalls / iterations, allocationCalls / iterations);
        }
        return 0;
    }
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
    // Windows reuses PIDs. Results must be new files, and a capture must come from this run.
    GUID runId{};
    wchar_t id[39]{};
    REQUIRE(SUCCEEDED(CoCreateGuid(&runId)) && StringFromGUID2(runId, id, _countof(id)));
    const auto outputDir = std::wstring(argv[1]) + L"-" + id;
    REQUIRE(CreateDirectoryW(outputDir.c_str(), nullptr));
    REQUIRE(CheckLookup() == 0);
    REQUIRE(CheckEntryAndLifetime(outputDir) == 0);
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
    allocationCalls = 0; failAllocation = 1;
    REQUIRE(StartApplication(L"C:\\child.exe", L"") == ERROR_NOT_ENOUGH_MEMORY);
    failAllocation = 0;
    allocationCalls = 0;
    REQUIRE(StartApplication(L"C:\\child.exe", L"--value=\"中文 空格\"") == ERROR_ACCESS_DENIED);
    REQUIRE(allocationCalls == 1 && liveAllocations == 0);
    REQUIRE(workingDirectory == L"C:\\" && application == L"C:\\child.exe");
    REQUIRE(forwarded == L"\"C:\\child.exe\" --value=\"中文 空格\"");
    REQUIRE(StartApplication(L"C:\\test folder\\child.exe", L"") == ERROR_ACCESS_DENIED);
    REQUIRE(workingDirectory == L"C:\\test folder");
    allocationCalls = 0;
    REQUIRE(StartApplication(L"C:\\child.exe", L"", L"D:\\working folder") == ERROR_ACCESS_DENIED);
    REQUIRE(workingDirectory == L"D:\\working folder" && allocationCalls == 1 && liveAllocations == 0);
    fakeCreate = false;
    DWORD missing = StartApplication(L"C:\\nonexistent-launcher-test-123\\missing.exe", L"");
    if (missing != ERROR_FILE_NOT_FOUND && missing != ERROR_PATH_NOT_FOUND && missing != ERROR_DIRECTORY) {
        printf("Unexpected error: %lu\n", missing);
        return 7;
    }

    // Quotes, spaces and Unicode in the path are received by an actual child, not a shell.
    unsigned index = 0;
    for (const auto tail : cases) {
        const auto capturePath = outputDir + L"\\capture with spaces-" + std::to_wstring(index++) + L".txt";
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
