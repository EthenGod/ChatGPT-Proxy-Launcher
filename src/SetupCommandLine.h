// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "AutomationIO.h"
#include "StartupIntegration.h"

namespace setup {
struct AutomationCommand {
    std::wstring operation, requestPath, resultPath;
    std::wstring manifestPath, entryId;
    bool replaceManifest = false;
    automation::Json request = automation::Json::Object();
};
// Separate parsing/planning from execution so tests supply isolated Paths and system callbacks.
void ParseAutomationCommand(const std::wstring& tail, AutomationCommand& command);
void CheckIsolatedPath(const Paths& paths, const std::wstring& path);
Options ResolveAutomationOptions(const automation::Json& request, const Paths& paths, const Loaded& loaded);
automation::Json RunAutomation(const AutomationCommand& command, const Paths& paths, Payload payload,
    StartupReport (*disableStartup)() = TryDisableOfficialStartup, bool isolated = false);
int RunSetupCommandLine(HINSTANCE instance, const std::wstring& arguments,
    const Paths* isolatedPaths = nullptr,
    StartupReport (*disableStartup)() = TryDisableOfficialStartup) noexcept;
}
