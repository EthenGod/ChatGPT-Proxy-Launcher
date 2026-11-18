// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "SetupCore.h"
#include "AutomationJson.h"

namespace setup::ui {
struct ProgramIconSource { std::wstring target; LinkIcon shortcut; };
// One PNG per resource or extension. Rows refer to the result by index.
automation::Json ProgramIcons(automation::Json& rows, const std::vector<ProgramIconSource>& sources,
    size_t remainingBytes, const std::wstring& isolatedRoot);
}
