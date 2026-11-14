// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "AutomationJson.h"
#include <cstdint>

namespace chain {
enum class ReplyKind { Ready, Failed, AlreadyRunning };
struct ReplyIdentity { std::wstring runId, attemptId, token; };
struct ReadyReply {
    ReplyKind kind = ReplyKind::Ready;
    std::wstring errorCode;
    bool retryable = false;
    uint32_t processId = 0;
    uint64_t processCreated = 0;
};
// Pure validation: no process handles, paths, clocks, or mutation of attempt state.
ReadyReply ParseReadyReply(const std::string& bytes, const ReplyIdentity& expected);
}
