// SPDX-License-Identifier: GPL-3.0-only
#include "../src/SetupCore.h"
#include <appmodel.h>
#include <tlhelp32.h>
#include <stdio.h>

// All discovery, input, focus and waits are replaced. No real application can be closed.
struct FakeProcess {
    DWORD pid = 2, parent = 0, session = 7;
    std::wstring name = L"ChatGPT.exe", family = L"OpenAI.Codex_2p2nqsd0c76g0";
    std::wstring image = L"C:\\Fixture\\app\\ChatGPT.exe";
    UINT_PTR user = 1;
    bool alive = true, exits = true, denied = false;
    ULONGLONG created = 100, exitAt = 0;
};
struct FakeWindow {
    HWND handle;
    DWORD pid;
    std::wstring name = L"Chrome_WidgetWin_1";
    bool visible = true, enabled = true, iconic = false, owner = false, tool = false;
};
static std::vector<FakeProcess> processes;
static std::vector<FakeWindow> windows;
static std::vector<DWORD> quitRequests;
static HWND foreground = nullptr;
static bool focusFails = false, focusIgnored = false, restartAfterQuit = false, timesFail = false;
static bool lateWorker = false, focusLost = false, exitOnFocus = false;
static int heldKey = 0;
static UINT inputResult = 4, cleanupKeys = 0;
static DWORD quitDelay = 0;
static size_t cursor = 0;
static ULONGLONG clockTicks = 0;
static bool safeAccess = true, enumFails = false, identityFails = false;
static unsigned acquired = 0, released = 0;
static bool registered = false, foreignPublisher = false, resourcePackage = false;
static LONG registeredError = ERROR_SUCCESS;
static unsigned registeredCalls = 0;
static HANDLE H(UINT_PTR id) { return reinterpret_cast<HANDLE>(id); }
static FakeProcess* Find(HANDLE handle) {
    for (auto& process : processes) if (process.pid == reinterpret_cast<UINT_PTR>(handle)) return &process;
    return nullptr;
}
static DWORD WINAPI CurrentId() { return 1; }
static HANDLE WINAPI CurrentProcess() { return H(1); }
static BOOL WINAPI Session(DWORD pid, DWORD* session) {
    if (pid == 1) { *session = 7; return TRUE; }
    auto p = Find(H(pid)); if (!p) return FALSE; *session = p->session; return TRUE;
}
static HANDLE WINAPI Snapshot(DWORD flags, DWORD pid) {
    safeAccess = safeAccess && flags == TH32CS_SNAPPROCESS && pid == 0;
    ++acquired; return H(9000);
}
static BOOL Entry(PROCESSENTRY32W* entry) {
    if (cursor >= processes.size()) { SetLastError(ERROR_NO_MORE_FILES); return FALSE; }
    const auto& p = processes[cursor]; entry->th32ProcessID = p.pid; entry->th32ParentProcessID = p.parent;
    wcscpy_s(entry->szExeFile, p.name.c_str()); return TRUE;
}
static BOOL WINAPI First(HANDLE, PROCESSENTRY32W* entry) { cursor = 0; return Entry(entry); }
static BOOL WINAPI Next(HANDLE, PROCESSENTRY32W* entry) { ++cursor; return Entry(entry); }
static HANDLE WINAPI Open(DWORD access, BOOL inherit, DWORD pid) {
    safeAccess = safeAccess && access == (PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE) && !inherit;
    auto p = Find(H(pid));
    if (!p || p->denied) { SetLastError(ERROR_ACCESS_DENIED); return nullptr; }
    ++acquired; return H(pid);
}
static BOOL WINAPI Close(HANDLE) { ++released; return TRUE; }
static BOOL WINAPI Token(HANDLE process, DWORD access, HANDLE* token) {
    safeAccess = safeAccess && access == TOKEN_QUERY;
    *token = H(reinterpret_cast<UINT_PTR>(process) + 10000); ++acquired; return TRUE;
}
static BOOL WINAPI TokenInfo(HANDLE token, TOKEN_INFORMATION_CLASS type, LPVOID data, DWORD size, DWORD* needed) {
    safeAccess = safeAccess && type == TokenUser;
    *needed = sizeof(TOKEN_USER);
    if (!data || size < *needed) { SetLastError(ERROR_INSUFFICIENT_BUFFER); return FALSE; }
    const UINT_PTR id = reinterpret_cast<UINT_PTR>(token) - 10000;
    auto p = Find(H(id));
    static_cast<TOKEN_USER*>(data)->User.Sid = reinterpret_cast<PSID>(p ? p->user : 1);
    return TRUE;
}
static BOOL WINAPI Sid(PSID a, PSID b) { return a == b; }
static LONG Copy(const std::wstring& value, UINT32* size, PWSTR buffer) {
    if (*size <= value.size()) { *size = static_cast<UINT32>(value.size() + 1); return ERROR_INSUFFICIENT_BUFFER; }
    wcscpy_s(buffer, *size, value.c_str()); *size = static_cast<UINT32>(value.size() + 1); return ERROR_SUCCESS;
}
static LONG WINAPI Family(HANDLE process, UINT32* size, PWSTR buffer) {
    if (identityFails) return ERROR_ACCESS_DENIED;
    auto p = Find(process); if (!p || p->family.empty()) return APPMODEL_ERROR_NO_PACKAGE;
    return Copy(p->family, size, buffer);
}
static LONG WINAPI FullName(HANDLE, UINT32* size, PWSTR buffer) { return Copy(L"VerifiedPackage", size, buffer); }
static LONG WINAPI PackagePath(PCWSTR, UINT32* size, PWSTR buffer) { return Copy(L"C:\\Fixture", size, buffer); }
static LONG WINAPI Packages(PCWSTR family, UINT32* count, PWSTR* names, UINT32* size, PWSTR buffer) {
    ++registeredCalls;
    safeAccess = safeAccess && wcscmp(family, L"OpenAI.Codex_2p2nqsd0c76g0") == 0;
    if (registeredError) return registeredError;
    if (!registered) { *count = *size = 0; return ERROR_SUCCESS; }
    const wchar_t fullName[] = L"VerifiedPackage";
    if (!names || !buffer || *count < 1 || *size < _countof(fullName)) {
        *count = 1; *size = _countof(fullName); return ERROR_INSUFFICIENT_BUFFER;
    }
    wcscpy_s(buffer, *size, fullName); names[0] = buffer;
    *count = 1; *size = _countof(fullName); return ERROR_SUCCESS;
}
static LONG WINAPI PackageId(PCWSTR, UINT32 flags, UINT32* bytes, BYTE* buffer) {
    safeAccess = safeAccess && flags == PACKAGE_INFORMATION_BASIC;
    if (!buffer || *bytes < sizeof(PACKAGE_ID)) { *bytes = sizeof(PACKAGE_ID); return ERROR_INSUFFICIENT_BUFFER; }
    auto* id = reinterpret_cast<PACKAGE_ID*>(buffer); *id = {};
    id->name = const_cast<PWSTR>(L"OpenAI.Codex");
    id->publisherId = const_cast<PWSTR>(foreignPublisher ? L"OtherPublisher" : L"2p2nqsd0c76g0");
    id->resourceId = const_cast<PWSTR>(resourcePackage ? L"resources" : L"");
    return ERROR_SUCCESS;
}
static BOOL WINAPI ImagePath(HANDLE process, DWORD, PWSTR buffer, DWORD* size) {
    UINT32 length = *size; const auto status = Copy(Find(process)->image, &length, buffer); *size = length;
    return status == ERROR_SUCCESS;
}
static FakeWindow* Window(HWND handle) {
    for (auto& window : windows) if (window.handle == handle) return &window;
    return nullptr;
}
static BOOL WINAPI Windows(WNDENUMPROC callback, LPARAM data) {
    if (enumFails) return FALSE;
    for (const auto& window : windows) callback(window.handle, data);
    return TRUE;
}
static DWORD WINAPI WindowProcess(HWND handle, DWORD* pid) { auto w = Window(handle); *pid = w ? w->pid : 0; return 1; }
static HWND WINAPI Owner(HWND handle, UINT) { auto w = Window(handle); return w && w->owner ? reinterpret_cast<HWND>(1) : nullptr; }
static LONG_PTR WINAPI Style(HWND handle, int) { auto w = Window(handle); return w && w->tool ? WS_EX_TOOLWINDOW : 0; }
static int WINAPI ClassName(HWND handle, PWSTR buffer, int length) {
    auto w = Window(handle); if (!w) return 0;
    wcscpy_s(buffer, length, w->name.c_str()); return static_cast<int>(w->name.size());
}
static BOOL WINAPI Visible(HWND handle) { auto w = Window(handle); return w && w->visible; }
static BOOL WINAPI Enabled(HWND handle) { auto w = Window(handle); return w && w->enabled; }
static BOOL WINAPI Iconic(HWND handle) { auto w = Window(handle); return w && w->iconic; }
static BOOL WINAPI Show(HWND handle, int command) {
    auto w = Window(handle);
    safeAccess = safeAccess && w && w->name == L"Chrome_WidgetWin_1" && command == SW_RESTORE;
    if (!w) return FALSE;
    w->visible = true; w->iconic = false; return TRUE;
}
static unsigned foregroundReads = 0;
static HWND WINAPI Foreground() {
    if (focusLost && foreground == reinterpret_cast<HWND>(20) && ++foregroundReads >= 2)
        foreground = reinterpret_cast<HWND>(999);
    return foreground;
}
static BOOL WINAPI Focus(HWND handle) {
    if (exitOnFocus) { Find(H(Window(handle)->pid))->alive = false; return FALSE; }
    if (focusFails) return FALSE;
    if (!focusIgnored) foreground = handle;
    return TRUE;
}
static SHORT WINAPI KeyState(int key) { return key == heldKey ? static_cast<SHORT>(0x8000) : 0; }
static BOOL WINAPI Times(HANDLE handle, FILETIME* created, FILETIME*, FILETIME*, FILETIME*) {
    auto p = Find(handle); if (!p || timesFail) return FALSE;
    created->dwLowDateTime = static_cast<DWORD>(p->created);
    created->dwHighDateTime = static_cast<DWORD>(p->created >> 32); return TRUE;
}
static UINT WINAPI Input(UINT count, INPUT* keys, int size) {
    safeAccess = safeAccess && size == sizeof(INPUT);
    if (count == 1) {
        safeAccess = safeAccess && keys[0].ki.dwFlags == KEYEVENTF_KEYUP &&
            ((keys[0].ki.wVk == 'Q' && inputResult >= 2) || (keys[0].ki.wVk == VK_CONTROL && inputResult >= 1));
        ++cleanupKeys; return 1;
    }
    const WORD codes[] = {VK_CONTROL, 'Q', 'Q', VK_CONTROL};
    safeAccess = safeAccess && count == 4;
    if (count != 4) return 0;
    for (UINT i = 0; i < count; ++i)
        safeAccess = safeAccess && keys[i].type == INPUT_KEYBOARD && keys[i].ki.wVk == codes[i]
            && keys[i].ki.dwFlags == (i >= 2 ? DWORD{KEYEVENTF_KEYUP} : DWORD{0});
    auto w = Window(foreground);
    safeAccess = safeAccess && w && w->name == L"Chrome_WidgetWin_1" && !heldKey;
    if (inputResult != count) return inputResult;
    if (!w) return 0;
    quitRequests.push_back(w->pid);
    for (auto& p : processes) if ((p.pid == w->pid || p.parent == w->pid) && p.exits) {
        if (quitDelay) p.exitAt = clockTicks + quitDelay; else p.alive = false;
    }
    if (restartAfterQuit) { FakeProcess p; p.pid = 90; processes.push_back(p); }
    if (lateWorker) {
        FakeProcess p; p.pid = 91; p.parent = 2; p.created = 300;
        p.name = L"codex.exe"; p.family.clear(); p.image = L"C:\\Runtime\\codex.exe";
        processes.push_back(p);
    }
    return count;
}
static DWORD WINAPI Wait(HANDLE handle, DWORD timeout) {
    safeAccess = safeAccess && timeout == 0;
    auto p = Find(handle); return p && p->alive ? WAIT_TIMEOUT : WAIT_OBJECT_0;
}
static ULONGLONG WINAPI Clock() { return clockTicks; }
static void WINAPI Pause(DWORD milliseconds) {
    clockTicks += milliseconds;
    for (auto& p : processes) if (p.exitAt && clockTicks >= p.exitAt) p.alive = false;
}
#define GetCurrentProcessId CurrentId
#define GetCurrentProcess CurrentProcess
#define ProcessIdToSessionId Session
#define CreateToolhelp32Snapshot Snapshot
#define Process32FirstW First
#define Process32NextW Next
#define OpenProcess Open
#define CloseHandle Close
#define OpenProcessToken Token
#define GetTokenInformation TokenInfo
#define EqualSid Sid
#define GetPackageFamilyName Family
#define GetPackageFullName FullName
#define GetPackagePathByFullName PackagePath
#define GetPackagesByPackageFamily Packages
#define PackageIdFromFullName PackageId
#define QueryFullProcessImageNameW ImagePath
#define EnumWindows Windows
#define GetWindowThreadProcessId WindowProcess
#define GetWindow Owner
#define GetWindowLongPtrW Style
#define GetClassNameW ClassName
#define IsWindowVisible Visible
#define IsWindowEnabled Enabled
#define IsIconic Iconic
#define ShowWindowAsync Show
#define GetForegroundWindow Foreground
#define SetForegroundWindow Focus
#define GetAsyncKeyState KeyState
#define GetProcessTimes Times
#define SendInput Input
// Guard against accidentally restoring the old message/force-exit approach.
#define PostMessageW(...) (safeAccess = false, FALSE)
#define SendMessageW(...) (safeAccess = false, LRESULT{0})
#define PostThreadMessageW(...) (safeAccess = false, FALSE)
#define TerminateProcess(...) (safeAccess = false, FALSE)
#define WaitForSingleObject Wait
#define GetTickCount64 Clock
#define Sleep Pause
#include "../src/CodexClose.cpp"

#define REQUIRE(condition) do { if (!(condition)) { printf("FAIL line %d: %s\n", __LINE__, #condition); return 20; } } while (0)
static HWND W(UINT_PTR value) { return reinterpret_cast<HWND>(value); }
static void Reset() {
    processes = {FakeProcess{}}; quitRequests.clear(); clockTicks = 0;
    windows = {{W(20), 2}, {W(21), 2, L"OwlElectron_NotifyIconHostWindow", false},
        {W(22), 2, L"Chrome_WidgetWin_0", false}, {W(23), 2, L"crashpad_SessionEndWatcher", false}};
    foreground = W(999); focusFails = focusIgnored = restartAfterQuit = timesFail = false;
    lateWorker = focusLost = exitOnFocus = false; foregroundReads = 0;
    heldKey = 0; inputResult = 4; cleanupKeys = 0; quitDelay = 0;
    safeAccess = true; enumFails = identityFails = false; acquired = released = 0;
    registered = foreignPublisher = resourcePackage = false; registeredError = ERROR_SUCCESS; registeredCalls = 0;
}
static void AddWorker(DWORD id, bool desktop, bool exits = true) {
    FakeProcess p; p.pid = id; p.parent = 2; p.created = 200; p.exits = exits;
    if (!desktop) { p.name = L"codex.exe"; p.family.clear(); p.image = L"C:\\Runtime\\codex.exe"; }
    processes.push_back(p); windows.push_back({W(200 + id), id, L"Chrome_WidgetWin_0", false});
}
int main() {
    using setup::ChatGPTCloseResult; using setup::TryCloseChatGPT;
    Reset(); AddWorker(3, true); AddWorker(4, false);
    AddWorker(5, false); processes.back().parent = 0; quitDelay = 500;
    REQUIRE(TryCloseChatGPT() == ChatGPTCloseResult::Closed && quitRequests == std::vector<DWORD>{2});
    REQUIRE(processes.back().alive && safeAccess && acquired == released && clockTicks == 500);
    Reset(); AddWorker(3, true); windows.back().name = L"Chrome_WidgetWin_1";
    windows.back().visible = true; foreground = windows.back().handle;
    REQUIRE(TryCloseChatGPT() == ChatGPTCloseResult::Closed && quitRequests == std::vector<DWORD>{2});
    REQUIRE(safeAccess && acquired == released);
    Reset(); AddWorker(3, true); processes.back().parent = 0;
    windows.back().name = L"Chrome_WidgetWin_1"; windows.back().visible = true;
    REQUIRE(TryCloseChatGPT() == ChatGPTCloseResult::Closed && quitRequests == std::vector<DWORD>({2, 3}));
    REQUIRE(safeAccess && acquired == released);
    Reset(); exitOnFocus = true;
    REQUIRE(TryCloseChatGPT() == ChatGPTCloseResult::Closed && quitRequests.empty());
    REQUIRE(safeAccess && acquired == released);
    for (const auto* name : {L"OwlElectron_NotifyIconHostWindow", L"Chrome_WidgetWin_0", L"crashpad_SessionEndWatcher"}) {
        Reset(); windows[0].name = name;
        REQUIRE(TryCloseChatGPT() == ChatGPTCloseResult::NeedsManualExit && quitRequests.empty());
        REQUIRE(safeAccess && acquired == released);
    }
    Reset(); windows.insert(windows.begin(), {W(19), 2, L"Chrome_WidgetWin_1", false});
    REQUIRE(TryCloseChatGPT() == ChatGPTCloseResult::Closed && foreground == W(20) && quitRequests.size() == 1);
    REQUIRE(safeAccess && acquired == released);
    Reset(); windows[0].visible = false; windows[0].iconic = true;
    REQUIRE(TryCloseChatGPT() == ChatGPTCloseResult::Closed && windows[0].visible && !windows[0].iconic);
    REQUIRE(safeAccess && acquired == released);
    for (int mode = 0; mode < 8; ++mode) {
        Reset();
        if (mode == 0) focusFails = true;
        if (mode == 1) focusIgnored = true;
        if (mode == 2) heldKey = VK_CONTROL;
        if (mode == 3) windows[0].enabled = false;
        if (mode == 4) windows[0].owner = true;
        if (mode == 5) windows[0].tool = true;
        if (mode == 6) timesFail = true;
        if (mode == 7) focusLost = true;
        REQUIRE(TryCloseChatGPT() == ChatGPTCloseResult::NeedsManualExit && quitRequests.empty());
        REQUIRE(safeAccess && acquired == released && clockTicks <= 500);
    }
    for (UINT count = 0; count < 4; ++count) {
        Reset(); inputResult = count;
        REQUIRE(TryCloseChatGPT() == ChatGPTCloseResult::NeedsManualExit && quitRequests.empty());
        REQUIRE(cleanupKeys == (count == 0 ? 0u : count == 1 ? 1u : 2u));
        REQUIRE(safeAccess && acquired == released);
    }
    for (int mode = 0; mode < 5; ++mode) {
        Reset();
        if (mode == 0) processes[0].exits = false;
        if (mode == 1) AddWorker(3, true, false);
        if (mode == 2) AddWorker(4, false, false);
        if (mode == 3) restartAfterQuit = true;
        if (mode == 4) lateWorker = true;
        REQUIRE(TryCloseChatGPT() == ChatGPTCloseResult::NeedsManualExit && quitRequests.size() == 1);
        REQUIRE(safeAccess && acquired == released && clockTicks <= 10000);
    }
    Reset(); AddWorker(4, false, false); processes.back().created = 50;
    REQUIRE(TryCloseChatGPT() == ChatGPTCloseResult::Closed && processes.back().alive);
    REQUIRE(safeAccess && acquired == released);
    for (int mode = 0; mode < 6; ++mode) {
        Reset(); auto& p = processes[0];
        if (mode == 0) p.family = L"Other.Publisher";
        if (mode == 1) p.family.clear();
        if (mode == 2) p.session = 8;
        if (mode == 3) p.user = 2;
        if (mode == 4) { p.name = L"codex.exe"; p.image = L"C:\\Other\\codex.exe"; }
        if (mode == 5) p.name = L"Other.exe";
        REQUIRE(TryCloseChatGPT() == ChatGPTCloseResult::NotRunning && quitRequests.empty());
        REQUIRE(safeAccess && acquired == released);
    }
    for (int mode = 0; mode < 3; ++mode) {
        Reset();
        if (mode == 0) processes[0].denied = true;
        if (mode == 1) enumFails = true;
        if (mode == 2) identityFails = true;
        REQUIRE(TryCloseChatGPT() == ChatGPTCloseResult::NeedsManualExit && quitRequests.empty());
        REQUIRE(safeAccess && acquired == released);
    }
    Reset(); registered = true; processes[0].family.clear();
    REQUIRE(TryCloseChatGPT() == ChatGPTCloseResult::Closed && quitRequests == std::vector<DWORD>{2});
    REQUIRE(registeredCalls == 2 && safeAccess && acquired == released);
    for (int mode = 0; mode < 6; ++mode) {
        Reset(); registered = true; processes[0].family.clear();
        if (mode == 0) processes[0].image = L"C:\\Fixture-copy\\app\\ChatGPT.exe";
        if (mode == 1) processes[0].user = 2;
        if (mode == 2) processes[0].session = 8;
        if (mode == 3) foreignPublisher = true;
        if (mode == 4) resourcePackage = true;
        if (mode == 5) registeredError = ERROR_ACCESS_DENIED;
        REQUIRE(TryCloseChatGPT() == (mode == 5 ? ChatGPTCloseResult::NeedsManualExit : ChatGPTCloseResult::NotRunning));
        REQUIRE(quitRequests.empty() && safeAccess && acquired == released);
    }
    Reset(); processes.clear();
    REQUIRE(TryCloseChatGPT() == ChatGPTCloseResult::NotRunning && safeAccess && acquired == released);
    puts("PASS application Quit: primary-window-only input, focus/modifier guards, partial-input cleanup and bounded wait");
    puts("PASS full exit: desktop and app-owned workers, lingering processes and restart detection, independent CLI exclusion");
    puts("PASS identity: package and exact registered path, publisher/resource/user/session checks and handle cleanup (mock OS only)");
    return 0;
}
