// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "AutomationIO.h"

namespace automation {
std::wstring ReadAppVersion(const std::wstring& executable);
Json CapabilityList(bool launcherOnly = false);
Json MakeDiscoveryRecord(const std::wstring& executable, const std::wstring& version);
// Read metadata only; never execute the target or read private launch arguments.
Json ReadDeploymentInfo(const std::wstring& root, const std::wstring& settingsLink);
}
