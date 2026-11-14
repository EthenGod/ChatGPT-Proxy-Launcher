// SPDX-License-Identifier: GPL-3.0-only
#include "CompanionProtocol.h"
#include "AutomationContract.h"

namespace chain {
ReadyReply ParseReadyReply(const std::string& bytes, const ReplyIdentity& expected) {
    using automation::Invalid;
    namespace rules = automation::contract;
    const auto reply = automation::ParseJson(bytes, rules::ReplyBytes);
    if (reply.At(L"schemaVersion").Integer(0, UINT32_MAX) != rules::ReplySchema
        || reply.At(L"runId").String() != expected.runId
        || reply.At(L"attemptId").String() != expected.attemptId
        || reply.At(L"token").String() != expected.token) throw Invalid("invalid-reply");
    ReadyReply result;
    const auto& status = reply.At(L"status").String();
    if (status == L"ready") {
        reply.Fields({L"schemaVersion", L"runId", L"attemptId", L"token", L"status"});
    } else if (status == L"failed") {
        reply.Fields({L"schemaVersion", L"runId", L"attemptId", L"token", L"status", L"errorCode", L"retryable", L"message"});
        result.kind = ReplyKind::Failed;
        result.errorCode = reply.At(L"errorCode").String(64);
        if (result.errorCode.empty() || result.errorCode[0] < L'a' || result.errorCode[0] > L'z'
            || result.errorCode.find_first_not_of(L"abcdefghijklmnopqrstuvwxyz0123456789-") != std::wstring::npos)
            throw Invalid("invalid-error-code");
        const bool requested = reply.At(L"retryable").Bool();
        result.retryable = requested && !rules::PermanentError(result.errorCode);
        if (reply.Has(L"message")) reply.At(L"message").String(256);
    } else if (status == L"already-running") {
        reply.Fields({L"schemaVersion", L"runId", L"attemptId", L"token", L"status", L"readyProcessId", L"readyProcessCreationTime"});
        result.kind = ReplyKind::AlreadyRunning;
        result.processId = static_cast<uint32_t>(reply.At(L"readyProcessId").Integer(1, UINT32_MAX));
        const auto& stamp = reply.At(L"readyProcessCreationTime").String(16);
        if (stamp.size() != 16 || stamp.find_first_not_of(L"0123456789abcdefABCDEF") != std::wstring::npos)
            throw Invalid("invalid-ready-process");
        for (auto c : stamp) {
            const unsigned digit = c >= L'0' && c <= L'9' ? c - L'0' : c >= L'a' && c <= L'f' ? c - L'a' + 10 : c - L'A' + 10;
            result.processCreated = (result.processCreated << 4) | digit;
        }
    } else throw Invalid("invalid-reply-status");
    return result;
}
}
