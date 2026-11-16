// SPDX-License-Identifier: GPL-3.0-only
// Native reference entry for process-level comparisons. No real user paths.
#include "SetupBridgeFixture.h"
#include "../src/SetupCommandLine.h"
#include "../src/Localization.h"
static setup::StartupReport NoStartup() { return {}; }
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR arguments, int) {
    try {
        wchar_t root[32768]{};
        const DWORD size = GetEnvironmentVariableW(L"CHATGPT_SETUP_TEST_ROOT", root, 32768);
        if (!size || size >= 32768 || !arguments || !*arguments) return 2;
        const auto paths = BridgeTestPaths(automation::FullPath(root));
        l10n::Select(L"zh-CN");
        return setup::RunSetupCommandLine(instance, arguments, &paths, NoStartup);
    } catch (...) { return 50; }
}
