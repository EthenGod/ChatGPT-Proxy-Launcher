// SPDX-License-Identifier: GPL-3.0-only
#include "SetupCore.h"
#include "CodexClose.h"
#include <appmodel.h>
#include <tlhelp32.h>
#include <algorithm>

namespace setup {
namespace close_detail {
struct Handle {
    HANDLE value = nullptr;
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};
struct Process { DWORD id; HANDLE handle; };
struct Targets {
    std::vector<Process> processes;
    bool uncertain = false;
    ~Targets() { for (const auto& process : processes) CloseHandle(process.handle); }
};
bool Same(const std::wstring& a, const std::wstring& b) {
    return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_EQUAL;
}
bool User(HANDLE process, std::vector<BYTE>& data) {
    Handle token;
    if (!OpenProcessToken(process, TOKEN_QUERY, &token.value)) return false;
    DWORD size = 0;
    GetTokenInformation(token.value, TokenUser, nullptr, 0, &size);
    if (!size || size > 65536) return false;
    data.resize(size);
    return GetTokenInformation(token.value, TokenUser, data.data(), size, &size) != FALSE;
}
enum class Identity { Other, Target, Unknown };
Identity Identify(HANDLE process, const std::vector<BYTE>& currentUser) {
    wchar_t family[256]{}; UINT32 count = _countof(family);
    const LONG status = GetPackageFamilyName(process, &count, family);
    if (status == APPMODEL_ERROR_NO_PACKAGE) return Identity::Other;
    if (status != ERROR_SUCCESS) return Identity::Unknown;
    if (wcscmp(family, L"OpenAI.Codex_2p2nqsd0c76g0")) return Identity::Other;
    wchar_t fullName[512]{}; count = _countof(fullName);
    if (GetPackageFullName(process, &count, fullName)) return Identity::Unknown;
    std::vector<wchar_t> root(32768), executable(32768);
    count = static_cast<UINT32>(root.size()); DWORD length = static_cast<DWORD>(executable.size());
    if (GetPackagePathByFullName(fullName, &count, root.data())
        || !QueryFullProcessImageNameW(process, 0, executable.data(), &length)) return Identity::Unknown;
    const std::wstring base = std::wstring(root.data()) + L"\\app\\";
    if (!Same(executable.data(), base + L"ChatGPT.exe") && !Same(executable.data(), base + L"Codex.exe")) return Identity::Other;
    std::vector<BYTE> owner;
    if (!User(process, owner)) return Identity::Unknown;
    return EqualSid(reinterpret_cast<const TOKEN_USER*>(owner.data())->User.Sid,
        reinterpret_cast<const TOKEN_USER*>(currentUser.data())->User.Sid) ? Identity::Target : Identity::Other;
}
BOOL CALLBACK CloseWindow(HWND window, LPARAM argument) {
    auto& targets = *reinterpret_cast<Targets*>(argument);
    DWORD pid = 0; GetWindowThreadProcessId(window, &pid);
    const auto found = std::find_if(targets.processes.begin(), targets.processes.end(),
        [pid](const Process& process) { return process.id == pid; });
    if (found == targets.processes.end() || GetWindow(window, GW_OWNER)
        || (GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TOOLWINDOW)) return TRUE;
    // Keep the verified process handle alive through dispatch and exit verification.
    // A window can disappear between enumeration and posting. The retained process
    // handles below decide whether exit succeeded, even when posting fails.
    if (WaitForSingleObject(found->handle, 0) == WAIT_TIMEOUT) PostMessageW(window, WM_CLOSE, 0, 0);
    return TRUE;
}
}
CodexCloseResult TryCloseCodex() {
    using namespace close_detail;
    DWORD session = 0;
    std::vector<BYTE> currentUser;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &session) || !User(GetCurrentProcess(), currentUser))
        return CodexCloseResult::NeedsManualExit;
    Handle snapshot; snapshot.value = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot.value == INVALID_HANDLE_VALUE) return CodexCloseResult::NeedsManualExit;
    Targets targets;
    PROCESSENTRY32W entry{}; entry.dwSize = sizeof(entry);
    if (!Process32FirstW(snapshot.value, &entry)) return CodexCloseResult::NeedsManualExit;
    do {
        if (!Same(entry.szExeFile, L"ChatGPT.exe") && !Same(entry.szExeFile, L"Codex.exe")) continue;
        DWORD otherSession = 0;
        if (!ProcessIdToSessionId(entry.th32ProcessID, &otherSession)) { targets.uncertain = true; continue; }
        if (otherSession != session || entry.th32ProcessID == GetCurrentProcessId()) continue;
        Handle process; process.value = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, entry.th32ProcessID);
        if (!process.value) { if (GetLastError() != ERROR_INVALID_PARAMETER) targets.uncertain = true; continue; }
        if (WaitForSingleObject(process.value, 0) == WAIT_OBJECT_0) continue;
        const Identity identity = Identify(process.value, currentUser);
        if (identity == Identity::Unknown) targets.uncertain = true;
        if (identity != Identity::Target) continue;
        if (targets.processes.size() >= 256) { targets.uncertain = true; break; }
        targets.processes.push_back({entry.th32ProcessID, process.value}); process.value = nullptr;
    } while (Process32NextW(snapshot.value, &entry));
    if (GetLastError() != ERROR_NO_MORE_FILES) targets.uncertain = true;
    if (targets.processes.empty()) return targets.uncertain ? CodexCloseResult::NeedsManualExit : CodexCloseResult::NotRunning;
    if (!EnumWindows(CloseWindow, reinterpret_cast<LPARAM>(&targets))) targets.uncertain = true;
    // One bounded wait in the existing save worker, never on the UI thread or at launcher startup.
    const ULONGLONG deadline = GetTickCount64() + 2000;
    for (;;) {
        bool pending = false;
        for (const auto& process : targets.processes) {
            const DWORD status = WaitForSingleObject(process.handle, 0);
            if (status == WAIT_FAILED) targets.uncertain = true;
            if (status != WAIT_OBJECT_0) pending = true;
        }
        if (!pending) return targets.uncertain ? CodexCloseResult::NeedsManualExit : CodexCloseResult::Closed;
        if (GetTickCount64() >= deadline) return CodexCloseResult::NeedsManualExit;
        Sleep(50);
    }
}
}
