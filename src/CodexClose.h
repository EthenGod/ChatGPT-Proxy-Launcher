// SPDX-License-Identifier: GPL-3.0-only
#pragma once
namespace setup {
enum class ChatGPTCloseResult { NotRunning, Closed, NeedsManualExit };
// Requests ChatGPT's application Quit command, then checks its process family.
// Never closes auxiliary windows or forcibly terminates any process.
ChatGPTCloseResult TryCloseChatGPT();
}
