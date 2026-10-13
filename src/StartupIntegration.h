// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "SetupCore.h"
namespace setup {
struct StartupReport {
    unsigned matched = 0, disabled = 0;
    DWORD error = 0;
    bool packagedTask = false; // Manifest declaration, not the user's enabled/disabled state.
    DWORD manifestError = 0;
    unsigned packagedTasks = 0, packagedDisabled = 0, packagedAlreadyOff = 0;
    DWORD packagedError = 0, restoreError = 0;
    std::wstring Message() const;
};
// Best effort, current user only. Never starts ChatGPT or impersonates its package identity.
StartupReport TryDisableOfficialStartup();
Result OpenStartupSettings(HWND owner);
} // namespace setup
