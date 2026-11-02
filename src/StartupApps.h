// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <windows.h>
#include <cwchar>

namespace companions {
inline constexpr wchar_t Root[] = L"ChatGPTProxyLauncher";
inline constexpr wchar_t Folder[] = L"StartupApps";
enum class Kind { Unknown, Exe, Link, Batch, PowerShell };
inline Kind FileKind(PCWSTR path) {
    PCWSTR ext = wcsrchr(path, L'.');
    if (!ext) return Kind::Unknown;
    if (!_wcsicmp(ext, L".exe")) return Kind::Exe;
    if (!_wcsicmp(ext, L".lnk")) return Kind::Link;
    if (!_wcsicmp(ext, L".bat") || !_wcsicmp(ext, L".cmd")) return Kind::Batch;
    if (!_wcsicmp(ext, L".ps1")) return Kind::PowerShell;
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
    return kind == Kind::Exe || kind == Kind::PowerShell || (kind == Kind::Batch && !*args);
}
using Start = DWORD (*)(PCWSTR exe, PCWSTR args, PCWSTR workingDirectory, int show);
void Run(PCWSTR chatExecutable, Start start) noexcept;
}
