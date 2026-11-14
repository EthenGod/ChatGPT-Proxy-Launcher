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
Json Run(const RunOptions& options, PCWSTR chat, companions::Start legacyStart) noexcept;
void NotifyFailures(const Json& report) noexcept;
}
