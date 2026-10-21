// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <windows.h>

// Best effort, no UI/logs. Never called by --launcher-check.
void RepairTaskbarShortcut(PCWSTR application) noexcept;
