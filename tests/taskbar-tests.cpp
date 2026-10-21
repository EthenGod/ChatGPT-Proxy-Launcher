// SPDX-License-Identifier: GPL-3.0-only
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#include <propkey.h>
#include <stdio.h>
#include <string>
#include <vector>

static unsigned moves = 0, copies = 0, notifications = 0, checks = 0;
static bool failMove = false, failCopy = false;
static unsigned folderCalls = 0, networkProbes = 0;
static HRESULT WINAPI TestFolder(REFKNOWNFOLDERID, DWORD, HANDLE, PWSTR* out) {
    ++folderCalls;
    const wchar_t path[] = L"\\\\invalid.example\\redirected-programs";
    *out = static_cast<PWSTR>(CoTaskMemAlloc(sizeof(path)));
    if (!*out) return E_OUTOFMEMORY;
    CopyMemory(*out, path, sizeof(path));
    return S_OK;
}
static DWORD WINAPI TestAttributes(PCWSTR path) {
    if (wcsncmp(path, L"\\\\", 2) == 0) {
        ++networkProbes; SetLastError(ERROR_BAD_NETPATH); return INVALID_FILE_ATTRIBUTES;
    }
    return GetFileAttributesW(path);
}
static BOOL WINAPI TestMove(PCWSTR from, PCWSTR to, DWORD flags) {
    ++moves;
    if (failMove && (flags & MOVEFILE_REPLACE_EXISTING)) { SetLastError(ERROR_ACCESS_DENIED); return FALSE; }
    return MoveFileExW(from, to, flags);
}
static BOOL WINAPI TestCopy(PCWSTR from, PCWSTR to, BOOL exclusive) {
    ++copies;
    if (failCopy) { SetLastError(ERROR_ACCESS_DENIED); return FALSE; }
    return CopyFileW(from, to, exclusive);
}
static void WINAPI TestNotify(LONG, UINT, LPCVOID, LPCVOID) { ++notifications; }
#define MoveFileExW TestMove
#define CopyFileW TestCopy
#define SHChangeNotify TestNotify
#define SHGetKnownFolderPath TestFolder
#define GetFileAttributesW TestAttributes
#include "../src/TaskbarShortcut.cpp"
#undef MoveFileExW
#undef CopyFileW
#undef SHChangeNotify
#undef SHGetKnownFolderPath
#undef GetFileAttributesW

#define REQUIRE(condition) do { ++checks; if (!(condition)) { printf("FAIL line %d: %s\n", __LINE__, #condition); return 20; } } while (0)
using namespace taskbar;
static const wchar_t* Old = L"C:\\Test Packages\\OpenAI.Codex_1.2.3.0_x64__2p2nqsd0c76g0\\app\\ChatGPT.exe";
static const wchar_t* Current = L"C:\\Test Packages\\OpenAI.Codex_2.0.0.0_x64__2p2nqsd0c76g0\\app\\ChatGPT.exe";
static std::vector<BYTE> Bytes(const std::wstring& path) {
    Handle file;
    file.h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (file.h == INVALID_HANDLE_VALUE) return {};
    const DWORD size = GetFileSize(file.h, nullptr);
    if (size == INVALID_FILE_SIZE || size > 1024 * 1024) return {};
    std::vector<BYTE> data(size); DWORD read = 0;
    if (!ReadFile(file.h, data.data(), size, &read, nullptr) || read != size) return {};
    return data;
}
static HRESULT Make(const std::wstring& path, const std::wstring& target, const std::wstring& args,
    PCWSTR id = AppId, PCWSTR icon = L"C:\\old icon.ico") {
    Com<IShellLinkW> link; Com<IPersistFile> persist; Com<IPropertyStore> props;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link.p));
    if (FAILED(hr)) return hr;
    if (FAILED(hr = link->SetPath(target.c_str())) || FAILED(hr = link->SetArguments(args.c_str()))
        || FAILED(hr = link->SetIconLocation(icon, 2))
        || FAILED(hr = link->SetWorkingDirectory(L"C:\\old dir"))
        || FAILED(hr = link->SetDescription(L"保留说明"))
        || FAILED(hr = link->SetShowCmd(SW_SHOWMINNOACTIVE))
        || FAILED(hr = link->SetHotkey(MAKEWORD('G', HOTKEYF_CONTROL | HOTKEYF_ALT)))) return hr;
    if (FAILED(hr = link->QueryInterface(IID_PPV_ARGS(&props.p)))) return hr;
    if (id) {
        PROPVARIANT value{}; value.vt = VT_LPWSTR; value.pwszVal = const_cast<PWSTR>(id);
        if (FAILED(hr = props->SetValue(PKEY_AppUserModel_ID, value)) || FAILED(hr = props->Commit())) return hr;
    }
    if (FAILED(hr = link->QueryInterface(IID_PPV_ARGS(&persist.p)))) return hr;
    return persist->Save(path.c_str(), TRUE);
}
static int Verify(const std::wstring& path, const std::wstring& launcher, const std::wstring& args) {
    Com<IShellLinkW> link; Com<IPersistFile> persist; Values value;
    REQUIRE(SUCCEEDED(Load(path, link, persist)));
    REQUIRE(CorrectId(link.p));
    REQUIRE(SUCCEEDED(Read(link.p, value)) && Stable(value, launcher));
    REQUIRE(value.arguments == args);
    wchar_t description[128]{}; int show = 0; WORD hotkey = 0;
    REQUIRE(SUCCEEDED(link->GetDescription(description, 128)) && wcscmp(description, L"保留说明") == 0);
    REQUIRE(SUCCEEDED(link->GetShowCmd(&show)) && show == SW_SHOWMINNOACTIVE);
    REQUIRE(SUCCEEDED(link->GetHotkey(&hotkey)) && hotkey == MAKEWORD('G', HOTKEYF_CONTROL | HOTKEYF_ALT));
    return 0;
}
int wmain(int argc, wchar_t** argv) {
    // Separate process timing, still restricted to a caller-provided test fixture.
    if (argc == 5 && wcscmp(argv[1], L"--repair-once") == 0)
        return RepairLink(argv[2], argv[3], argv[4], Current) == S_FALSE ? 0 : 30;
    REQUIRE(argc == 3);
    Apartment apartment;
    REQUIRE(SUCCEEDED(apartment.hr));
    RepairTaskbarShortcut(Current);
    REQUIRE(folderCalls == 1 && networkProbes == 0);
    const std::wstring root = std::wstring(argv[1]) + L"\\taskbar-tests-" + std::to_wstring(GetCurrentProcessId())
        + L"-" + std::to_wstring(GetTickCount64()) + L" 中文";
    REQUIRE(CreateDirectoryW(root.c_str(), nullptr));
    const std::wstring launcher = root + L"\\launcher with spaces.exe";
    REQUIRE(CopyFileW(argv[2], launcher.c_str(), TRUE));
    const std::wstring path = root + L"\\ChatGPT.lnk", backup = root + L"\\backup\\original.lnk";
    const std::wstring args = L"--proxy-server=http://127.0.0.1:7897  --value=\"中文 空格\\\"引用\" \"\" --url=%TEMP%";
    REQUIRE(RepairLink(path, backup, launcher, Current) == S_FALSE); // absent: no creation
    REQUIRE(GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES);
    REQUIRE(PackageTarget(Old, Current));
    for (PCWSTR bad : { L"C:\\Other\\OpenAI.Codex_1.2.3.0_x64__2p2nqsd0c76g0\\app\\ChatGPT.exe",
        L"C:\\Test Packages\\OpenAI.Other_1.2.3.0_x64__2p2nqsd0c76g0\\app\\ChatGPT.exe",
        L"C:\\Test Packages\\OpenAI.Codex_1.2.3.0_x64_en-us_2p2nqsd0c76g0\\app\\ChatGPT.exe",
        L"C:\\Test Packages\\OpenAI.Codex_1.2.3.0_x64__aaaaaaaaaaaaa\\app\\ChatGPT.exe",
        L"C:\\Test Packages\\garbage\\app\\ChatGPT.exe", L"\\\\host\\share\\ChatGPT.exe",
        L"C:\\Test Packages\\OpenAI.Codex_1.2.3.0_x64__2p2nqsd0c76g0\\app\\other.exe" }) {
        REQUIRE(!PackageTarget(bad, Current));
        REQUIRE(SUCCEEDED(Make(path, bad, args)));
        const auto before = Bytes(path);
        REQUIRE(RepairLink(path, backup, launcher, Current) == S_FALSE && Bytes(path) == before);
    }
    for (PCWSTR id : { L"other.app", static_cast<PCWSTR>(nullptr) }) {
        REQUIRE(SUCCEEDED(Make(path, Old, args, id)));
        const auto before = Bytes(path);
        REQUIRE(RepairLink(path, backup, launcher, Current) == S_FALSE && Bytes(path) == before);
    }
    REQUIRE(SUCCEEDED(Make(path, Old, args)));
    const auto original = Bytes(path); REQUIRE(!original.empty());
    failCopy = true;
    REQUIRE(FAILED(RepairLink(path, backup, launcher, Current)) && Bytes(path) == original);
    failCopy = false; failMove = true;
    REQUIRE(FAILED(RepairLink(path, backup, launcher, Current)) && Bytes(path) == original);
    REQUIRE(Bytes(backup) == original && notifications == 0);
    failMove = false;
    REQUIRE(RepairLink(path, backup, launcher, Current) == S_OK);
    REQUIRE(Verify(path, launcher, args) == 0);
    REQUIRE(notifications == 1);
    const auto stable = Bytes(path); const unsigned moved = moves, copied = copies;
    WIN32_FILE_ATTRIBUTE_DATA beforeTime{}, afterTime{};
    REQUIRE(GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &beforeTime));
    DWORD handlesBefore = 0, handlesAfter = 0;
    REQUIRE(GetProcessHandleCount(GetCurrentProcess(), &handlesBefore));
    LARGE_INTEGER begin{}, end{}, frequency{};
    QueryPerformanceFrequency(&frequency); QueryPerformanceCounter(&begin);
    for (unsigned i = 0; i < 100; ++i) REQUIRE(RepairLink(path, backup, launcher, Current) == S_FALSE);
    QueryPerformanceCounter(&end);
    REQUIRE(GetProcessHandleCount(GetCurrentProcess(), &handlesAfter));
    REQUIRE(handlesAfter <= handlesBefore + 1);
    REQUIRE(GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &afterTime));
    REQUIRE(CompareFileTime(&beforeTime.ftLastWriteTime, &afterTime.ftLastWriteTime) == 0);
    REQUIRE(Bytes(path) == stable && moves == moved && copies == copied && notifications == 1);
    printf("Unchanged shortcut check: %.3f ms mean (100 in-process calls; excludes process startup).\n",
        (end.QuadPart - begin.QuadPart) * 1000.0 / frequency.QuadPart / 100.0);
    // Simulate the updater replacing the link; keep the first backup untouched.
    for (const auto& sample : { std::wstring(), args, std::wstring(8191, L'中') }) {
        REQUIRE(SUCCEEDED(Make(path, Current, sample)));
        REQUIRE(RepairLink(path, backup, launcher, Current) == S_OK);
        REQUIRE(Verify(path, launcher, sample) == 0);
        REQUIRE(Bytes(backup) == original);
    }
    REQUIRE(SUCCEEDED(Make(path, Old, args)));
    const auto lockedBytes = Bytes(path);
    {
        Handle lock;
        lock.h = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        REQUIRE(lock.h != INVALID_HANDLE_VALUE);
        REQUIRE(FAILED(RepairLink(path, backup, launcher, Current)));
    }
    REQUIRE(Bytes(path) == lockedBytes);
    const std::wstring blocked = root + L"\\backup-directory.lnk";
    REQUIRE(CreateDirectoryW(blocked.c_str(), nullptr));
    REQUIRE(FAILED(RepairLink(path, blocked, launcher, Current)) && Bytes(path) == lockedBytes);
    REQUIRE(RepairLink(path, backup, std::wstring(260, L'x'), Current) == S_FALSE);
    REQUIRE(RepairLink(path, backup, root + L"\\missing.exe", Current) == S_FALSE);
    REQUIRE(RepairLink(path, backup, Current, Current) == S_FALSE);
    REQUIRE(RepairLink(root, backup, launcher, Current) == S_FALSE);
    const std::wstring corrupt = root + L"\\corrupt.lnk";
    REQUIRE(CopyFileW(launcher.c_str(), corrupt.c_str(), TRUE));
    const auto corruptBytes = Bytes(corrupt);
    REQUIRE(FAILED(RepairLink(corrupt, backup, launcher, Current)) && Bytes(corrupt) == corruptBytes);
    REQUIRE(FAILED(RepairLink(path, corrupt, launcher, Current)) && Bytes(path) == lockedBytes);
    REQUIRE(Bytes(corrupt) == corruptBytes);
    REQUIRE(RepairLink(path, backup, launcher, Current) == S_OK);
    printf("PASS: taskbar shortcut repair (%u checks); real user shortcuts untouched.\n", checks);
    return 0;
}
