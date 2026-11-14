// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "CompanionManifest.h"

namespace chain {
std::wstring RootDirectory();
Json DeploymentInfo(const std::wstring& root);
struct RunOptions {
    std::wstring root, launcher;
    int feedback = -1; // -1 policy, 0 quiet, 1 notify.
};
// Report construction can allocate; the launcher owns the final exception boundary.
Json Run(const RunOptions& options, PCWSTR chat, companions::Start legacyStart);
void NotifyFailures(const Json& report) noexcept;
}
