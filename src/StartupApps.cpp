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
bool Extension(PCWSTR name, PCWSTR ext) {
    PCWSTR dot = wcsrchr(name, L'.');
    return dot && !_wcsicmp(dot, ext);
}
bool LocalPath(PCWSTR path) {
    return path && ((path[0] >= L'A' && path[0] <= L'Z') || (path[0] >= L'a' && path[0] <= L'z'))
        && path[1] == L':' && path[2] == L'\\';
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
        const bool link = Extension(file.cFileName, L".lnk");
        if (!link && !Extension(file.cFileName, L".exe")) continue;
        const std::wstring path = folder + L"\\" + file.cFileName;
        PCWSTR exe = path.c_str(), args = L"", directory = nullptr;
        int show = -1;
        if (link) {
            if (!com.Ready()) continue;
            ComPtr<IShellLinkW> shortcut; ComPtr<IPersistFile> persist;
            if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&shortcut.p)))
                || FAILED(shortcut.p->QueryInterface(IID_PPV_ARGS(&persist.p)))
                || FAILED(persist.p->Load(path.c_str(), STGM_READ))) continue;
            constexpr int capacity = 32768;
            if (buffers.empty()) buffers.resize(3 * capacity);
            auto target = buffers.data(), tail = target + capacity, cwd = tail + capacity;
            *target = *tail = *cwd = 0;
            if (FAILED(shortcut.p->GetPath(target, capacity, nullptr, 0)) || !LocalPath(target) || !Extension(target, L".exe")
                || FAILED(shortcut.p->GetArguments(tail, capacity)) || FAILED(shortcut.p->GetWorkingDirectory(cwd, capacity))
                || (*cwd && !LocalPath(cwd))) continue;
            if (FAILED(shortcut.p->GetShowCmd(&show))) show = -1;
            exe = target; args = tail; directory = *cwd ? cwd : nullptr;
        }
        PCWSTR leaf = wcsrchr(exe, L'\\') + 1;
        if (!_wcsicmp(exe, chat) || !_wcsicmp(leaf, L"ChatGPTProxyLauncher.exe") || !_wcsicmp(leaf, L"ChatGPTLauncherSetup.exe")) continue;
        // Descendants must not run the companion list again, including renamed copies of this launcher.
        if (!guard.set && !(guard.set = SetEnvironmentVariableW(Guard, L"1") != FALSE)) return;
        start(exe, args, directory, show); // A broken entry does not block the next one.
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
