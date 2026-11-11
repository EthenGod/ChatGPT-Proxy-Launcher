// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "SetupCore.h"

namespace setup {
// Merge bytes only. The caller validates options and owns file I/O and rollback.
// Unrelated content is preserved; output is replaced only after a successful merge.
Result MergeCodexEnv(const std::vector<BYTE>& source, const Options& options,
    std::vector<BYTE>& output);
} // namespace setup
