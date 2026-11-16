// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "SetupCore.h"
#include <string_view>

namespace setup {
inline bool IsCompatibilityCommand(std::wstring_view tail) noexcept {
    const auto end = tail.find_last_not_of(L" \t");
    if (end == tail.npos) return false;
    tail = tail.substr(0, end + 1);
    return tail == L"--compatibility" || tail == L"\"--compatibility\"";
}
// Internal isolated paths also lock the preview against writes and system actions.
int RunNativeSetup(HINSTANCE instance, int show, bool compatibility,
    const Paths* isolatedPaths = nullptr) noexcept;
}
