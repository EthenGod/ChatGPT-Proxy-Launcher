// SPDX-License-Identifier: GPL-3.0-only
#include "LauncherAutomation.h"
#include "CloudProxy.h"
#include "Localization.h"
#include "AppVersion.h"
#include "DeploymentDiscovery.h"
#include "NativeMemory.h"
#include <shlobj.h>
#include <shellapi.h>
#include <commctrl.h>

namespace chain {
namespace {

bool Token(PCWSTR& cursor, std::wstring& value) {
    while (*cursor == L' ' || *cursor == L'\t') ++cursor;
    if (!*cursor) return false;
    PCWSTR start = cursor; bool quoted = false, hasQuote = false; size_t slashes = 0;
    for (; *cursor; ++cursor) {
        const wchar_t c = *cursor;
        if (!quoted && (c == L' ' || c == L'\t')) break;
        if (c == L'"') { hasQuote = true; if (slashes % 2 == 0) quoted = !quoted; }
        slashes = c == L'\\' ? slashes + 1 : 0;
    }
    if (quoted) throw automation::Invalid("unclosed-option-quote");
    if (!hasQuote) { value.assign(start, cursor); return true; }
    int count = 0; auto argv = CommandLineToArgvW((L"launcher " + std::wstring(start, cursor)).c_str(), &count);
    if (!argv) throw automation::Invalid("invalid-option");
    native::LocalPtr<LPWSTR> ownedArguments(argv);
    if (count == 2) value = argv[1];
    if (count != 2) throw automation::Invalid("invalid-option"); return true;
}
bool MayStartWith(PCWSTR cursor, std::wstring_view prefix) {
    while (*cursor == L' ' || *cursor == L'\t') ++cursor;
    for (const wchar_t c : prefix) {
        while (*cursor == L'"') ++cursor;
        if (*cursor != c) return false;
        ++cursor;
    }
    return true;
}

int ReadDefaultProxy(Invocation& invocation, PCWSTR cursor) {
    // The setup form (including earlier versions) puts its selected proxy first.
    if (invocation.mode == Invocation::Mode::Check || !MayStartWith(cursor, cloudproxy::ServerPrefix)) return 0;
    try {
        std::wstring first;
        if (!Token(cursor, first) || first.compare(0, cloudproxy::ServerPrefix.size(), cloudproxy::ServerPrefix) != 0) return 0;
        const auto endpoint = std::wstring_view(first).substr(cloudproxy::ServerPrefix.size());
        if (!cloudproxy::ValidEndpoint(endpoint)) return 0;
        std::wstring token;
        while (Token(cursor, token) && token != L"--") {
            if (cloudproxy::ConflictingSwitch(token)) return 0;
        }
        invocation.cloudProxy = endpoint;
    } catch (const automation::Invalid&) {
        // An ambiguous raw command line still belongs to the client. Leave its environment inherited.
    }
    return 0;
}

int ReadDisabledProxy(Invocation& invocation, PCWSTR cursor) {
    while (*cursor == L' ' || *cursor == L'\t') ++cursor;
    invocation.forwarded = cursor;
    if (MayStartWith(cursor, L"--launcher-")) {
        std::wstring key;
        if (Token(cursor, key) && (key == L"--launcher-check" || key == L"--launcher-info" ||
            key == L"--launcher-chain" || key == cloudproxy::OffSwitch)) return 20;
    }
    return 0;
}

int ReadForwarded(Invocation& invocation, PCWSTR cursor) {
    while (*cursor == L' ' || *cursor == L'\t') ++cursor;
    invocation.forwarded = cursor;
    if (MayStartWith(cursor, cloudproxy::OffSwitch)) {
        std::wstring token;
        if (Token(cursor, token) && token == cloudproxy::OffSwitch) return ReadDisabledProxy(invocation, cursor);
    }
    return ReadDefaultProxy(invocation, invocation.forwarded);
}

HRESULT CALLBACK NotifyCallback(HWND window, UINT notification, WPARAM elapsed, LPARAM, LONG_PTR) {
    if (notification == TDN_TIMER && elapsed >= automation::contract::NotificationMs) SendMessageW(window, TDM_CLICK_BUTTON, IDCLOSE, 0);
    return S_OK;
}
}
Json DeploymentInfo(const std::wstring& root) {
    return automation::ReadDeploymentInfo(root, Join(root, automation::contract::SettingsLink));
}
int Invocation::Parse(PCWSTR raw) noexcept {
    forwarded = raw;
    cloudProxy.clear();
    try {
        // Ordinary launch retains the complete original tail and does not initialize JSON or COM.
        PCWSTR cursor = raw;
        // Windows allows quotes around any part of an option. Skip obvious non-options
        // cheaply; Token decodes the actual first argument and locates its raw tail.
        if (!MayStartWith(cursor, L"--launcher-")) return ReadDefaultProxy(*this, cursor);
        std::wstring token;
        if (!Token(cursor, token)) return 0;
        if (token == cloudproxy::OffSwitch) return ReadDisabledProxy(*this, cursor);
        if (token == L"--launcher-check") {
            mode = Mode::Check;
            return ReadForwarded(*this, cursor);
        }
        if (token == L"--launcher-info") { mode = Mode::Info; if (Token(cursor, token)) return 20; return 0; }
        if (token != L"--launcher-chain") return 0;
        mode = Mode::Chain; std::wstring path; bool feedback = false, delimiter = false;
        while (Token(cursor, token)) {
            if (token == L"--") { delimiter = true; break; }
            const auto key = token; if (!Token(cursor, token)) return 20;
            if (key == L"--result" && path.empty()) path = automation::FullPath(token);
            else if (key == L"--feedback" && !feedback) { feedback = true; if (token != L"quiet" && token != L"notify") return 20; options.feedback = token == L"notify" ? 1 : 0; }
            else return 20;
        }
        if (!delimiter || path.size() < 5 || _wcsicmp(path.c_str() + path.size() - 5, L".json")) return 20;
        if (const int status = ReadForwarded(*this, cursor)) return status;
        options.root = RootDirectory();
        const auto registry = Join(options.root, automation::contract::RegistryFolder) + L"\\";
        if (automation::SamePath(path, Join(options.root, automation::contract::DiscoveryFile)) || automation::SamePath(path, Join(options.root, automation::contract::PolicyFile))
            || (path.size() >= registry.size() && automation::SamePath(path.substr(0, registry.size()), registry))) return 20;
        output.Reserve(path); return 0;
    } catch (const automation::Invalid&) { return 20; } catch (...) { return 70; }
}
int Invocation::Info() noexcept {
    try {
        auto result = DeploymentInfo(RootDirectory()); result[L"currentLauncherVersion"] = Json(automation::FromUtf8(APP_VERSION_TEXT));
        result[L"currentCapabilities"] = automation::CapabilityList(true);
        const auto text = automation::WriteJson(result); const HANDLE stream = GetStdHandle(STD_OUTPUT_HANDLE); DWORD written = 0;
        if (!stream || stream == INVALID_HANDLE_VALUE || !WriteFile(stream, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) || written != text.size()) return 70;
        return 0;
    } catch (...) { return 50; }
}
int Invocation::Finish(DWORD nativeError, const Json& report) noexcept {
    try {
        Json result = nativeError ? Json::Object() : report;
        if (nativeError) { result[L"entries"] = Json::Array(); result[L"chainStatus"] = Json(L"not-started"); result[L"exitCode"] = Json::Number(50); }
        result[L"schemaVersion"] = Json::Number(automation::contract::ChainProtocol); result[L"operation"] = Json(L"chain-run");
        auto chat = Json::Object(); chat[L"processCreated"] = Json(nativeError == 0); chat[L"nativeError"] = Json::Number(nativeError); result[L"chatgpt"] = chat;
        output.Write(result); return static_cast<int>(result.At(L"exitCode").Integer(0, 255));
    } catch (...) { return 70; }
}
void NotifyFailures(const Json& report) noexcept {
    try {
        l10n::Initialize();
        USEROBJECTFLAGS flags{}; DWORD needed = 0;
        if (!GetUserObjectInformationW(GetProcessWindowStation(), UOI_FLAGS, &flags, sizeof(flags), &needed) || !(flags.dwFlags & WSF_VISIBLE)) return;
        std::wstring text; unsigned shown = 0;
        for (const auto& item : report.At(L"entries").array) {
            const auto status = item.At(L"status").String();
            if (automation::contract::ItemSucceeded(status)) continue;
            if (shown++ == 4) { text += l10n::Text("chain.more"); break; }
            text += item.At(L"id").String(160) + L": ";
            text += status == L"timeout-running" ? l10n::Text("chain.timeout") : status == L"skipped-dependency" ? l10n::Text("chain.dependency") : status == L"skipped-budget" ? l10n::Text("chain.budget") : l10n::Text("chain.failed");
            text += L"\n";
        }
        TASKDIALOGCONFIG config{sizeof(config)}; config.dwFlags = TDF_CALLBACK_TIMER | TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT;
        config.dwCommonButtons = TDCBF_CLOSE_BUTTON; config.pszWindowTitle = l10n::Text("common.launcher"); config.pszMainInstruction = l10n::Text("chain.title");
        config.pszContent = text.c_str(); config.pszFooter = l10n::Text("chain.autoclose"); config.pfCallback = NotifyCallback;
        TaskDialogIndirect(&config, nullptr, nullptr, nullptr);
    } catch (...) {}
}
}
