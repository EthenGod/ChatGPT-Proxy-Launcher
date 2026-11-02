// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <windows.h>
#include <cwchar>
#include <string>

namespace companions {
inline constexpr wchar_t Root[] = L"ChatGPTProxyLauncher";
inline constexpr wchar_t Folder[] = L"StartupApps";
enum class Kind { Unknown, Exe, Link, Batch, PowerShell, Wsh, Python, PythonWindow, Node, Jar, Count };
struct Format { PCWSTR ext; Kind kind; };
inline constexpr Format Formats[] = {
    {L".exe", Kind::Exe}, {L".lnk", Kind::Link}, {L".bat", Kind::Batch}, {L".cmd", Kind::Batch},
    {L".ps1", Kind::PowerShell}, {L".vbs", Kind::Wsh}, {L".js", Kind::Wsh}, {L".wsf", Kind::Wsh},
    {L".py", Kind::Python}, {L".pyw", Kind::PythonWindow}, {L".cjs", Kind::Node}, {L".mjs", Kind::Node}, {L".jar", Kind::Jar}
};
inline Kind FileKind(PCWSTR path) {
    PCWSTR ext = wcsrchr(path, L'.');
    if (!ext) return Kind::Unknown;
    for (const auto& f : Formats) if (!_wcsicmp(ext, f.ext)) return f.kind;
    return Kind::Unknown;
}
inline bool LocalPath(PCWSTR path) {
    return path && ((path[0] >= L'A' && path[0] <= L'Z') || (path[0] >= L'a' && path[0] <= L'z'))
        && path[1] == L':' && path[2] == L'\\' && !wcschr(path, L'"') && !wcspbrk(path, L"\r\n");
}
inline bool ValidTarget(PCWSTR path, PCWSTR args, PCWSTR cwd) {
    if (!LocalPath(path) || (*cwd && !LocalPath(cwd))) return false;
    PCWSTR leaf = wcsrchr(path, L'\\') + 1;
    if (!_wcsicmp(leaf, L"ChatGPTProxyLauncher.exe") || !_wcsicmp(leaf, L"ChatGPTLauncherSetup.exe")) return false;
    const auto kind = FileKind(path);
    return kind != Kind::Unknown && kind != Kind::Link && (kind != Kind::Batch || !*args);
}
// Only inspect explicit local PATH entries. Do not search the current directory, use file
// associations, trigger WindowsApps installation aliases, or modify the user's environment.
inline PCWSTR PathHost(Kind kind) {
    switch (kind) {
    case Kind::Python: return L"python.exe";
    case Kind::PythonWindow: return L"pythonw.exe";
    case Kind::Node: return L"node.exe";
    case Kind::Jar: return L"java.exe";
    default: return nullptr;
    }
}
inline bool HostFile(const std::wstring& path) {
    DWORD a = GetFileAttributesW(path.c_str());
    if (a == INVALID_FILE_ATTRIBUTES || (a & FILE_ATTRIBUTE_DIRECTORY)) return false;
    if (!(a & FILE_ATTRIBUTE_REPARSE_POINT)) return true;
    WIN32_FIND_DATAW f{};
    HANDLE h = FindFirstFileW(path.c_str(), &f);
    if (h == INVALID_HANDLE_VALUE) return false;
    FindClose(h);
    return f.dwReserved0 == IO_REPARSE_TAG_SYMLINK;
}
inline std::wstring FindHost(Kind kind) {
    if (PCWSTR name = PathHost(kind)) {
        DWORD n = GetEnvironmentVariableW(L"PATH", nullptr, 0);
        if (!n || n > 32768) return {};
        std::wstring paths(n, L'\0');
        DWORD got = GetEnvironmentVariableW(L"PATH", &paths[0], n);
        if (!got || got >= n) return {};
        paths.resize(got);
        for (size_t i = 0; i < paths.size();) {
            const auto end = paths.find(L';', i);
            auto part = paths.substr(i, end == paths.npos ? paths.npos : end - i);
            if (part.size() > 1 && part.front() == L'"' && part.back() == L'"') part = part.substr(1, part.size() - 2);
            if (LocalPath(part.c_str())) {
                const auto host = part + L"\\" + name;
                if (HostFile(host)) return host;
            }
            if (end == paths.npos) break;
            i = end + 1;
        }
        return {};
    }
    PCWSTR suffix = kind == Kind::PowerShell ? L"\\WindowsPowerShell\\v1.0\\powershell.exe" :
        kind == Kind::Batch ? L"\\cmd.exe" : kind == Kind::Wsh ? L"\\wscript.exe" : nullptr;
    wchar_t system[MAX_PATH];
    const UINT n = suffix ? GetSystemDirectoryW(system, MAX_PATH) : 0;
    if (!n || n >= MAX_PATH) return {};
    auto host = std::wstring(system) + suffix;
    return HostFile(host) ? host : std::wstring{};
}
using Start = DWORD (*)(PCWSTR exe, PCWSTR args, PCWSTR workingDirectory, int show);
void Run(PCWSTR chatExecutable, Start start) noexcept;
}
