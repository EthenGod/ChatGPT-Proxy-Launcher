// SPDX-License-Identifier: GPL-3.0-only
#pragma once
namespace setup {
enum class CodexCloseResult { NotRunning, Closed, NeedsManualExit };
// Requests normal window close after a successful env save. Never terminates a process.
CodexCloseResult TryCloseCodex();
}
