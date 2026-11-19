// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "SetupCore.h"
#include "StartupIntegration.h"
#include "CodexClose.h"

namespace setup::ui {
bool ReadCodexEnvPreference();
bool SaveCodexEnvPreference(bool enabled);
bool ReadDarkPreference();
bool SaveDarkPreference(bool dark);
bool SaveLanguagePreference();
// The caller owns an STA apartment. Cancellation succeeds with an empty path.
Result ChoosePath(HWND owner, bool program, const std::wstring& initial, std::wstring& path);
// Called only after a committed deployment. Failures cannot undo that commit.
void CompleteSave(bool startup, bool codexEnv, StartupReport& report, ChatGPTCloseResult& closed) noexcept;
}
