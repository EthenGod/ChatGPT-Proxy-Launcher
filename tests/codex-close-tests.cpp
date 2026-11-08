// SPDX-License-Identifier: GPL-3.0-only
#include "../src/SetupCore.h"
#include <appmodel.h>
#include <tlhelp32.h>
#include <stdio.h>

// All discovery, window messages and waits are replaced. No real application can be closed.
struct FakeProcess {
    DWORD pid = 2, session = 7;
    std::wstring name = L"ChatGPT.exe", family = L"OpenAI.Codex_2p2nqsd0c76g0";
    std::wstring image = L"C:\\Fixture\\app\\ChatGPT.exe";
    UINT_PTR user = 1;
    bool alive = true, window = true, exits = true, denied = false, sendFails = false, owner = false, tool = false;
    bool exitsBeforePost = false;
};
static std::vector<FakeProcess> processes;
static std::vector<DWORD> messages;
static size_t cursor = 0;
static ULONGLONG clockTicks = 0;
static bool safeAccess = true, enumFails = false, identityFails = false;
static unsigned acquired = 0, released = 0;
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
    const auto& p = processes[cursor]; entry->th32ProcessID = p.pid;
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
static BOOL WINAPI ImagePath(HANDLE process, DWORD, PWSTR buffer, DWORD* size) {
    UINT32 length = *size; const auto status = Copy(Find(process)->image, &length, buffer); *size = length;
    return status == ERROR_SUCCESS;
}
static BOOL WINAPI Windows(WNDENUMPROC callback, LPARAM data) {
    if (enumFails) return FALSE;
    for (const auto& p : processes) if (p.window) callback(reinterpret_cast<HWND>(static_cast<UINT_PTR>(p.pid)), data);
    return TRUE;
}
static DWORD WINAPI WindowProcess(HWND window, DWORD* pid) { *pid = static_cast<DWORD>(reinterpret_cast<UINT_PTR>(window)); return 1; }
static HWND WINAPI Owner(HWND window, UINT) { return Find(window)->owner ? reinterpret_cast<HWND>(1) : nullptr; }
static LONG_PTR WINAPI Style(HWND window, int) { return Find(window)->tool ? WS_EX_TOOLWINDOW : 0; }
static BOOL WINAPI Post(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    safeAccess = safeAccess && message == WM_CLOSE && wparam == 0 && lparam == 0;
    auto p = Find(window); messages.push_back(p->pid);
    if (p->exitsBeforePost) { p->alive = false; SetLastError(ERROR_INVALID_WINDOW_HANDLE); return FALSE; }
    if (p->sendFails) return FALSE;
    if (p->exits) p->alive = false;
    return TRUE;
}
static DWORD WINAPI Wait(HANDLE process, DWORD timeout) {
    safeAccess = safeAccess && timeout == 0;
    return Find(process)->alive ? WAIT_TIMEOUT : WAIT_OBJECT_0;
}
static ULONGLONG WINAPI Clock() { return clockTicks; }
static void WINAPI Pause(DWORD milliseconds) { clockTicks += milliseconds; }
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
#define QueryFullProcessImageNameW ImagePath
#define EnumWindows Windows
#define GetWindowThreadProcessId WindowProcess
#define GetWindow Owner
#define GetWindowLongPtrW Style
#define PostMessageW Post
#define WaitForSingleObject Wait
#define GetTickCount64 Clock
#define Sleep Pause
#include "../src/CodexClose.cpp"

#define REQUIRE(condition) do { if (!(condition)) { printf("FAIL line %d: %s\n", __LINE__, #condition); return 20; } } while (0)
static void Reset() {
    processes = {FakeProcess{}}; messages.clear(); clockTicks = 0;
    safeAccess = true; enumFails = identityFails = false; acquired = released = 0;
}
int main() {
    using setup::CodexCloseResult; using setup::TryCloseCodex;
    Reset(); REQUIRE(TryCloseCodex() == CodexCloseResult::Closed && messages == std::vector<DWORD>{2});
    REQUIRE(safeAccess && acquired == released && clockTicks == 0);
    Reset(); processes[0].exitsBeforePost = true;
    REQUIRE(TryCloseCodex() == CodexCloseResult::Closed);
    REQUIRE(safeAccess && acquired == released && clockTicks == 0);
    for (int mode = 0; mode < 6; ++mode) {
        Reset(); auto& p = processes[0];
        if (mode == 0) p.family = L"Other.Publisher";
        if (mode == 1) p.family.clear();
        if (mode == 2) p.session = 8;
        if (mode == 3) p.user = 2;
        if (mode == 4) { p.name = L"codex.exe"; p.image = L"C:\\Other\\codex.exe"; }
        if (mode == 5) p.name = L"Other.exe";
        REQUIRE(TryCloseCodex() == CodexCloseResult::NotRunning && messages.empty());
        REQUIRE(safeAccess && acquired == released);
    }
    for (int mode = 0; mode < 6; ++mode) {
        Reset(); auto& p = processes[0];
        if (mode == 0) p.exits = false; // Tray hiding/refused close is not process exit.
        if (mode == 1) p.sendFails = true;
        if (mode == 2) p.window = false;
        if (mode == 3) p.denied = true;
        if (mode == 4) enumFails = true;
        if (mode == 5) identityFails = true;
        REQUIRE(TryCloseCodex() == CodexCloseResult::NeedsManualExit);
        REQUIRE(safeAccess && acquired == released && clockTicks <= 2000);
        if (mode >= 2) REQUIRE(messages.empty());
    }
    Reset(); processes.push_back(FakeProcess{}); processes[1].pid = 3;
    processes[1].name = L"Codex.exe"; processes[1].image = L"C:\\Fixture\\app\\Codex.exe";
    REQUIRE(TryCloseCodex() == CodexCloseResult::Closed && messages.size() == 2);
    REQUIRE(safeAccess && acquired == released);
    puts("PASS Codex close: package/path/user/session identity, CLI exclusion, normal exit, tray refusal, access failure, bounded wait and handle cleanup (mock OS only)");
    return 0;
}
