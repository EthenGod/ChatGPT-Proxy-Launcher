// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "../src/SetupCore.h"
#include "../src/AutomationJson.h"

inline setup::Paths BridgeTestPaths(const std::wstring& root) {
    using setup::Join;
    return {Join(root, L"state"), Join(root, L"startup.lnk"), Join(root, L"state\\settings.lnk"),
        Join(root, L"desktop.lnk"), Join(root, L"codex.env"),
        Join(root, L"legacy-startup.lnk"), Join(root, L"legacy-desktop.lnk")};
}
inline std::string BridgeTestPathJson(const setup::Paths& paths) {
    using automation::Json;
    auto j = Json::Object();
    j[L"defaultDirectory"] = Json(paths.defaultDirectory);
    j[L"startupLink"] = Json(paths.startupLink); j[L"settingsLink"] = Json(paths.settingsLink);
    j[L"desktopLink"] = Json(paths.desktopLink); j[L"codexEnvFile"] = Json(paths.codexEnvFile);
    j[L"legacyStartupLink"] = Json(paths.legacyStartupLink); j[L"legacyDesktopLink"] = Json(paths.legacyDesktopLink);
    return automation::WriteJson(j);
}
