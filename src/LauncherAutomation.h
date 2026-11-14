// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "CompanionRunner.h"
namespace chain {
struct Invocation {
    enum class Mode { Normal, Info, Chain } mode = Mode::Normal;
    PCWSTR forwarded = nullptr;
    RunOptions options;
    automation::ResultFile output;
    int Parse(PCWSTR raw) noexcept;
    int Info() noexcept;
    int Finish(DWORD nativeError, const Json& chainReport) noexcept;
};
}
