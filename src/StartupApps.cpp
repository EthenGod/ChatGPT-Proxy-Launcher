// SPDX-License-Identifier: GPL-3.0-only
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "StartupApps.h"
#include <shlobj.h>
#include <string>
#include <vector>

namespace companions {
namespace {
template<class T> struct ComPtr {
    T* p = nullptr;
    ~ComPtr() { if (p) p->Release(); }
};
struct Search {
    HANDLE h;
    ~Search() { if (h != INVALID_HANDLE_VALUE) FindClose(h); }
};
struct Apartment {
    HRESULT hr = E_PENDING;
    bool Ready() {
        if (hr == E_PENDING) hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        return SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
    }
    ~Apartment() { if (SUCCEEDED(hr)) CoUninitialize(); }
};
constexpr wchar_t Guard[] = L"CHATGPT_PROXY_LAUNCHER_COMPANION";
struct ChildGuard {
    bool set = false;
    ~ChildGuard() { if (set) SetEnvironmentVariableW(Guard, nullptr); }
};
void StartTarget(Kind kind, PCWSTR target, PCWSTR args, PCWSTR cwd, int show, Start start) {
    if (kind == Kind::Exe) { start(target, args, cwd, show); return; }
    wchar_t system[MAX_PATH];
    UINT n = GetSystemDirectoryW(system, MAX_PATH);
    if (!n || n >= MAX_PATH) return;
    std::wstring parent;
    if (!cwd) {
        const auto split = wcsrchr(target, L'\\') - target;
        parent.assign(target, split == 2 ? 3 : static_cast<size_t>(split));
        cwd = parent.c_str();
    }
    if (kind == Kind::PowerShell) {
        const std::wstring host = std::wstring(system) + L"\\WindowsPowerShell\\v1.0\\powershell.exe";
        std::wstring tail = std::wstring(L"-NoLogo -NoProfile -File \"") + target + L"\"";
        if (*args) { tail += L' '; tail += args; }
        start(host.c_str(), tail.c_str(), cwd, show);
    } else if (kind == Kind::Batch) {
        const std::wstring host = std::wstring(system) + L"\\cmd.exe";
        // Fixed command only. Expand the quoted path once, with delayed expansion and AutoRun off.
        // Never append a shortcut's user-supplied argument tail to cmd /c.
        constexpr wchar_t var[] = L"CHATGPT_PROXY_LAUNCHER_BATCH_PATH";
        if (SetEnvironmentVariableW(var, target)) {
            start(host.c_str(), L"/d /v:off /s /c \"\"%CHATGPT_PROXY_LAUNCHER_BATCH_PATH%\"\"", cwd, show);
            SetEnvironmentVariableW(var, nullptr);
        }
    }
}
void Dispatch(const std::wstring& folder, PCWSTR chat, Start start) {
    if (!LocalPath(folder.c_str())) return;
    wchar_t flag[2];
    if (GetEnvironmentVariableW(Guard, flag, 2)) return;
    const DWORD attrs = GetFileAttributesW(folder.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY) || (attrs & FILE_ATTRIBUTE_REPARSE_POINT)) return;
    WIN32_FIND_DATAW file{};
    Search search{FindFirstFileExW((folder + L"\\*").c_str(), FindExInfoBasic, &file, FindExSearchNameMatch, nullptr, 0)};
    if (search.h == INVALID_HANDLE_VALUE) return;
    Apartment com;
    ChildGuard guard;
    // Allocate shortcut buffers once, and only when there is a shortcut to read.
    std::vector<wchar_t> buffers;
    do {
        if (file.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) continue;
        auto kind = FileKind(file.cFileName);
        if (kind == Kind::Unknown) continue;
        const std::wstring path = folder + L"\\" + file.cFileName;
        PCWSTR exe = path.c_str(), args = L"", directory = nullptr;
        int show = -1;
        if (kind == Kind::Link) {
            if (!com.Ready()) continue;
            ComPtr<IShellLinkW> shortcut; ComPtr<IPersistFile> persist;
            if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&shortcut.p)))
                || FAILED(shortcut.p->QueryInterface(IID_PPV_ARGS(&persist.p)))
                || FAILED(persist.p->Load(path.c_str(), STGM_READ))) continue;
            constexpr int capacity = 32768;
            if (buffers.empty()) buffers.resize(3 * capacity);
            auto target = buffers.data(), tail = target + capacity, cwd = tail + capacity;
            *target = *tail = *cwd = 0;
            if (FAILED(shortcut.p->GetPath(target, capacity, nullptr, 0))
                || FAILED(shortcut.p->GetArguments(tail, capacity)) || FAILED(shortcut.p->GetWorkingDirectory(cwd, capacity))) continue;
            if (FAILED(shortcut.p->GetShowCmd(&show))) show = -1;
            exe = target; args = tail; directory = *cwd ? cwd : nullptr; kind = FileKind(target);
        }
        if (!_wcsicmp(exe, chat) || !ValidTarget(exe, args, directory ? directory : L"")) continue;
        // Descendants must not run the companion list again, including renamed copies of this launcher.
        if (!guard.set && !(guard.set = SetEnvironmentVariableW(Guard, L"1") != FALSE)) return;
        StartTarget(kind, exe, args, directory, show, start); // A broken entry does not block the next one.
    } while (FindNextFileW(search.h, &file));
}
}
void Run(PCWSTR chat, Start start) noexcept {
    try {
        struct Known { PWSTR p = nullptr; ~Known() { CoTaskMemFree(p); } } local;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DONT_VERIFY, nullptr, &local.p)))
            Dispatch(std::wstring(local.p) + L"\\" + Root + L"\\" + Folder, chat, start);
    } catch (...) {} // Optional programs never change the outcome of launching ChatGPT.
}
}
