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
struct Process {
    DWORD id, parent;
    HANDLE handle;
    ULONGLONG created;
    bool desktop;
    HWND window = nullptr;
};
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
struct RegisteredApps {
    bool loaded = false, valid = false;
    std::vector<std::wstring> executables;
};
bool ReadRegisteredApps(RegisteredApps& apps) {
    apps.loaded = true;
    UINT32 count = 0, characters = 0;
    LONG status = GetPackagesByPackageFamily(L"OpenAI.Codex_2p2nqsd0c76g0", &count, nullptr, &characters, nullptr);
    if (status == ERROR_SUCCESS && !count) return true;
    if (status != ERROR_INSUFFICIENT_BUFFER || !count || count > 1024 || !characters || characters > 2097152) return false;
    std::vector<PWSTR> names(count);
    std::vector<wchar_t> strings(characters);
    status = GetPackagesByPackageFamily(L"OpenAI.Codex_2p2nqsd0c76g0", &count, names.data(), &characters, strings.data());
    if (status != ERROR_SUCCESS || count > names.size()) return false;
    for (UINT32 i = 0; i < count; ++i) {
        UINT32 bytes = 0;
        status = PackageIdFromFullName(names[i], PACKAGE_INFORMATION_BASIC, &bytes, nullptr);
        if (status != ERROR_INSUFFICIENT_BUFFER || bytes < sizeof(PACKAGE_ID) || bytes > 131072) return false;
        std::vector<BYTE> data(bytes);
        if (PackageIdFromFullName(names[i], PACKAGE_INFORMATION_BASIC, &bytes, data.data())) return false;
        const auto* id = reinterpret_cast<const PACKAGE_ID*>(data.data());
        if (!id->name || !id->publisherId || !id->resourceId) return false;
        if (wcscmp(id->name, L"OpenAI.Codex") || wcscmp(id->publisherId, L"2p2nqsd0c76g0") || *id->resourceId) continue;
        std::vector<wchar_t> root(32768);
        UINT32 length = static_cast<UINT32>(root.size());
        if (GetPackagePathByFullName(names[i], &length, root.data())) return false;
        const std::wstring base = std::wstring(root.data()) + L"\\app\\";
        apps.executables.push_back(base + L"ChatGPT.exe");
        apps.executables.push_back(base + L"Codex.exe");
    }
    return true;
}
Identity Identify(HANDLE process, const std::vector<BYTE>& currentUser, RegisteredApps& apps) {
    wchar_t family[256]{}; UINT32 count = _countof(family);
    const LONG status = GetPackageFamilyName(process, &count, family);
    if (status != ERROR_SUCCESS && status != APPMODEL_ERROR_NO_PACKAGE) return Identity::Unknown;
    if (status == ERROR_SUCCESS && wcscmp(family, L"OpenAI.Codex_2p2nqsd0c76g0")) return Identity::Other;
    std::vector<wchar_t> executable(32768);
    DWORD length = static_cast<DWORD>(executable.size());
    if (!QueryFullProcessImageNameW(process, 0, executable.data(), &length)) return Identity::Unknown;
    if (status == APPMODEL_ERROR_NO_PACKAGE) {
        // Direct EXE launches can lack package identity. Accept only an exact path
        // obtained from the current user's registered, verified application package.
        if (!apps.loaded) apps.valid = ReadRegisteredApps(apps);
        if (!apps.valid) return Identity::Unknown;
        if (std::none_of(apps.executables.begin(), apps.executables.end(),
            [&](const std::wstring& path) { return Same(path, executable.data()); })) return Identity::Other;
    } else {
        wchar_t fullName[512]{}; count = _countof(fullName);
        if (GetPackageFullName(process, &count, fullName)) return Identity::Unknown;
        std::vector<wchar_t> root(32768); count = static_cast<UINT32>(root.size());
        if (GetPackagePathByFullName(fullName, &count, root.data())) return Identity::Unknown;
        const std::wstring base = std::wstring(root.data()) + L"\\app\\";
        if (!Same(executable.data(), base + L"ChatGPT.exe") && !Same(executable.data(), base + L"Codex.exe")) return Identity::Other;
    }
    std::vector<BYTE> owner;
    if (!User(process, owner)) return Identity::Unknown;
    return EqualSid(reinterpret_cast<const TOKEN_USER*>(owner.data())->User.Sid,
        reinterpret_cast<const TOKEN_USER*>(currentUser.data())->User.Sid) ? Identity::Target : Identity::Other;
}
const Process* Find(const Targets& targets, DWORD id) {
    for (const auto& process : targets.processes) if (process.id == id) return &process;
    return nullptr;
}
Process* Find(Targets& targets, DWORD id) {
    for (auto& process : targets.processes) if (process.id == id) return &process;
    return nullptr;
}
bool CreationTime(HANDLE process, ULONGLONG& value) {
    FILETIME created{}, exited{}, kernel{}, user{};
    if (!GetProcessTimes(process, &created, &exited, &kernel, &user)) return false;
    value = (static_cast<ULONGLONG>(created.dwHighDateTime) << 32) | created.dwLowDateTime;
    return true;
}
bool SameUser(HANDLE process, const std::vector<BYTE>& currentUser) {
    std::vector<BYTE> owner;
    return User(process, owner) && EqualSid(reinterpret_cast<const TOKEN_USER*>(owner.data())->User.Sid,
        reinterpret_cast<const TOKEN_USER*>(currentUser.data())->User.Sid);
}
bool DesktopName(PCWSTR name) { return Same(name, L"ChatGPT.exe") || Same(name, L"Codex.exe"); }
bool WorkerName(PCWSTR name) {
    return Same(name, L"codex.exe") || Same(name, L"codex-code-mode-host.exe") || Same(name, L"codex-command-runner.exe");
}
void Discover(Targets& targets, DWORD session, const std::vector<BYTE>& currentUser, RegisteredApps& apps,
    const Targets* previous = nullptr) {
    Handle snapshot; snapshot.value = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot.value == INVALID_HANDLE_VALUE) { targets.uncertain = true; return; }
    std::vector<PROCESSENTRY32W> entries;
    PROCESSENTRY32W entry{}; entry.dwSize = sizeof(entry);
    if (!Process32FirstW(snapshot.value, &entry)) {
        targets.uncertain = GetLastError() != ERROR_NO_MORE_FILES; return;
    }
    do {
        if (!DesktopName(entry.szExeFile) && !WorkerName(entry.szExeFile)) continue;
        if (entries.size() >= 1024) { targets.uncertain = true; return; }
        entries.push_back(entry);
    } while (Process32NextW(snapshot.value, &entry));
    if (GetLastError() != ERROR_NO_MORE_FILES) { targets.uncertain = true; return; }
    auto parentOf = [&](DWORD id) -> const Process* {
        const Process* parent = Find(targets, id);
        return parent ? parent : previous ? Find(*previous, id) : nullptr;
    };
    auto add = [&](const PROCESSENTRY32W& item, bool desktop) {
        if (item.th32ProcessID == GetCurrentProcessId()) return false;
        DWORD otherSession = 0;
        if (!ProcessIdToSessionId(item.th32ProcessID, &otherSession)) { targets.uncertain = true; return false; }
        if (session != otherSession) return false;
        Handle process; process.value = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, item.th32ProcessID);
        if (!process.value) { if (GetLastError() != ERROR_INVALID_PARAMETER) targets.uncertain = true; return false; }
        const DWORD state = WaitForSingleObject(process.value, 0);
        if (state == WAIT_OBJECT_0) return false;
        if (state != WAIT_TIMEOUT) { targets.uncertain = true; return false; }
        if (desktop) {
            const Identity identity = Identify(process.value, currentUser, apps);
            if (identity == Identity::Unknown) targets.uncertain = true;
            if (identity != Identity::Target) return false;
        } else if (!SameUser(process.value, currentUser)) { targets.uncertain = true; return false; }
        ULONGLONG created = 0;
        if (!CreationTime(process.value, created)) { targets.uncertain = true; return false; }
        if (!desktop) {
            const Process* parent = parentOf(item.th32ParentProcessID);
            if (!parent || created < parent->created) return false; // Parent PID was reused.
        }
        if (targets.processes.size() >= 256) { targets.uncertain = true; return false; }
        targets.processes.push_back({item.th32ProcessID, item.th32ParentProcessID, process.value, created, desktop});
        process.value = nullptr; return true;
    };
    for (const auto& item : entries) if (DesktopName(item.szExeFile)) add(item, true);
    // Observe app-owned workers as well. Never send them messages or include an independent CLI.
    for (unsigned pass = 0; pass < 64; ++pass) {
        bool added = false;
        for (const auto& item : entries) if (WorkerName(item.szExeFile) && !Find(targets, item.th32ProcessID)
            && parentOf(item.th32ParentProcessID)) added = add(item, false) || added;
        if (!added) return;
    }
    targets.uncertain = true;
}
bool MainWindow(HWND window, DWORD pid) {
    DWORD owner = 0; GetWindowThreadProcessId(window, &owner);
    wchar_t name[64]{};
    return owner == pid && !GetWindow(window, GW_OWNER)
        && !(GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TOOLWINDOW)
        && GetClassNameW(window, name, _countof(name)) && wcscmp(name, L"Chrome_WidgetWin_1") == 0;
}
BOOL CALLBACK ChooseWindow(HWND window, LPARAM argument) {
    auto& targets = *reinterpret_cast<Targets*>(argument);
    DWORD pid = 0; GetWindowThreadProcessId(window, &pid);
    Process* process = Find(targets, pid);
    if (!process || !process->desktop || !MainWindow(window, pid)) return TRUE;
    const Process* parent = Find(targets, process->parent);
    if (parent && parent->desktop && parent->created <= process->created) return TRUE;
    if (!process->window || (IsWindowVisible(window) && !IsWindowVisible(process->window))
        || GetForegroundWindow() == window) process->window = window;
    return TRUE;
}
bool KeysReleased() {
    const int keys[] = {VK_CONTROL, VK_SHIFT, VK_MENU, VK_LWIN, VK_RWIN, 'Q'};
    for (int key : keys)
        if (GetAsyncKeyState(key) & 0x8000) return false;
    return true;
}
bool RequestQuit(const Process& process) {
    const HWND window = process.window;
    if (WaitForSingleObject(process.handle, 0) == WAIT_OBJECT_0) return true;
    if (!MainWindow(window, process.id) || !IsWindowEnabled(window) || !KeysReleased()) return false;
    if (!IsWindowVisible(window) || IsIconic(window)) {
        if (!ShowWindowAsync(window, SW_RESTORE)) return false;
    }
    if (!SetForegroundWindow(window)) return false;
    const ULONGLONG deadline = GetTickCount64() + 500;
    while (GetForegroundWindow() != window && GetTickCount64() < deadline) Sleep(25);
    // Send the native Quit shortcut in one batch after foreground and identity checks.
    // Do not post WM_CLOSE to any window, including Chromium and tray infrastructure.
    if (!MainWindow(window, process.id) || !KeysReleased()
        || WaitForSingleObject(process.handle, 0) != WAIT_TIMEOUT) return false;
    INPUT keys[4]{};
    const WORD codes[] = {VK_CONTROL, 'Q', 'Q', VK_CONTROL};
    for (size_t i = 0; i < _countof(keys); ++i) {
        keys[i].type = INPUT_KEYBOARD; keys[i].ki.wVk = codes[i];
        if (i >= 2) keys[i].ki.dwFlags = KEYEVENTF_KEYUP;
    }
    if (GetForegroundWindow() != window) return false;
    const UINT sent = SendInput(_countof(keys), keys, sizeof(INPUT));
    if (sent != _countof(keys)) {
        // Release only keys introduced by a partial batch; never leave Ctrl held down.
        if (sent >= 2) SendInput(1, &keys[2], sizeof(INPUT));
        if (sent >= 1) SendInput(1, &keys[3], sizeof(INPUT));
        return false;
    }
    return true;
}
} // namespace close_detail
ChatGPTCloseResult TryCloseChatGPT() {
    using namespace close_detail;
    DWORD session = 0; std::vector<BYTE> user;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &session) || !User(GetCurrentProcess(), user))
        return ChatGPTCloseResult::NeedsManualExit;
    Targets targets; RegisteredApps apps;
    Discover(targets, session, user, apps);
    if (targets.uncertain) return ChatGPTCloseResult::NeedsManualExit;
    if (targets.processes.empty()) return ChatGPTCloseResult::NotRunning;
    if (!EnumWindows(ChooseWindow, reinterpret_cast<LPARAM>(&targets))) return ChatGPTCloseResult::NeedsManualExit;
    unsigned requested = 0;
    for (const auto& process : targets.processes) if (process.window) {
        if (!RequestQuit(process) && WaitForSingleObject(process.handle, 0) != WAIT_OBJECT_0)
            return ChatGPTCloseResult::NeedsManualExit;
        ++requested;
    }
    if (!requested) return ChatGPTCloseResult::NeedsManualExit;
    const ULONGLONG deadline = GetTickCount64() + 10000;
    for (;;) {
        bool pending = false;
        for (const auto& process : targets.processes) {
            const DWORD state = WaitForSingleObject(process.handle, 0);
            if (state == WAIT_FAILED) return ChatGPTCloseResult::NeedsManualExit;
            pending = pending || state != WAIT_OBJECT_0;
        }
        if (!pending) {
            // Retain ancestry for a worker spawned during shutdown, even after its parent exits.
            Targets remaining; Discover(remaining, session, user, apps, &targets);
            return !remaining.uncertain && remaining.processes.empty() ? ChatGPTCloseResult::Closed : ChatGPTCloseResult::NeedsManualExit;
        }
        if (GetTickCount64() >= deadline) return ChatGPTCloseResult::NeedsManualExit;
        Sleep(100);
    }
}
}
