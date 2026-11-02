// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <windows.h>

namespace companions {
inline constexpr wchar_t Root[] = L"ChatGPTProxyLauncher";
inline constexpr wchar_t Folder[] = L"StartupApps";
using Start = DWORD (*)(PCWSTR exe, PCWSTR args, PCWSTR workingDirectory, int show);
void Run(PCWSTR chatExecutable, Start start) noexcept;
}
