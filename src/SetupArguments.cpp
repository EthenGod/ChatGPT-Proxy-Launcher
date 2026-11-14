// SPDX-License-Identifier: GPL-3.0-only
#include "SetupCore.h"
#include "Localization.h"
#include "AutomationIO.h"
#include <shellapi.h>
#include <cwchar>

namespace setup {
namespace {
Result Fail(DWORD code, const std::wstring& what) {
    return { code ? code : ERROR_GEN_FAILURE, what + L"\n" + ErrorText(code) };
}
struct Token { std::wstring value, raw; size_t end = 0; };
struct LocalArguments {
    LPWSTR* values;
    explicit LocalArguments(LPWSTR* value) : values(value) {}
    ~LocalArguments() { if (values) LocalFree(values); }
    LocalArguments(const LocalArguments&) = delete;
    LocalArguments& operator=(const LocalArguments&) = delete;
};
Result Tokens(const std::wstring& arguments, std::vector<Token>& tokens) {
    tokens.clear();
    if (arguments.find_first_of(L"\r\n") != std::wstring::npos
        || arguments.find(L'\0') != std::wstring::npos)
        return Fail(ERROR_INVALID_PARAMETER, l10n::Text("error.arguments.characters"));
    size_t i = 0;
    while (i < arguments.size()) {
        if (arguments[i] == L' ' || arguments[i] == L'\t') { ++i; continue; }
        const size_t start = i;
        bool quoted = false;
        size_t slashes = 0;
        for (; i < arguments.size(); ++i) {
            const wchar_t c = arguments[i];
            if (!quoted && (c == L' ' || c == L'\t')) break;
            if (c == L'"' && slashes % 2 == 0) quoted = !quoted;
            slashes = c == L'\\' ? slashes + 1 : 0;
        }
        if (quoted) return Fail(ERROR_INVALID_PARAMETER, l10n::Text("error.arguments.quote"));
        const std::wstring raw = arguments.substr(start, i - start);
        int count = 0;
        const LocalArguments argv(CommandLineToArgvW((L"program " + raw).c_str(), &count));
        if (!argv.values) return Fail(GetLastError(), l10n::Text("error.arguments.parse"));
        if (count != 2) return Fail(ERROR_INVALID_DATA, l10n::Text("error.arguments.format"));
        tokens.push_back({ argv.values[1], raw, i });
    }
    return {};
}
enum ManagedOption { Other, ProxyServer, SilentStart, ProxyBypass, ManagedOptionCount };
ManagedOption ManagedKind(const std::wstring& arg) {
    const std::wstring key = arg.substr(0, arg.find(L'='));
    if (key == L"--proxy-server") return ProxyServer;
    if (key == L"--no-startup-window") return SilentStart;
    if (key == L"--proxy-bypass-list") return ProxyBypass;
    return Other;
}
void Append(std::wstring& text, const std::wstring& item) {
    if (item.empty()) return;
    if (!text.empty()) text += L' ';
    text += item;
}
} // namespace

std::wstring QuoteArgument(const std::wstring& value) {
    return automation::Quote(value);
}
Result BuildArguments(const Options& options, std::wstring& arguments) {
    arguments.clear();
    if (options.proxy) {
        if (options.protocol != L"http" && options.protocol != L"https" && options.protocol != L"socks5")
            return Fail(ERROR_INVALID_PARAMETER, l10n::Text("error.proxy.protocol"));
        if (options.host.empty() || options.host.find_first_of(L" \t\r\n/\\\"@;=?#") != std::wstring::npos
            || options.host.find(L'\0') != std::wstring::npos)
            return Fail(ERROR_INVALID_PARAMETER, l10n::Text("error.proxy.host"));
        if (options.host.find(L':') != std::wstring::npos
            && (options.host.front() != L'[' || options.host.back() != L']'))
            return Fail(ERROR_INVALID_PARAMETER, l10n::Text("error.proxy.ipv6"));
        if (options.port.empty() || options.port.size() > 5
            || options.port.find_first_not_of(L"0123456789") != std::wstring::npos)
            return Fail(ERROR_INVALID_PARAMETER, l10n::Text("error.proxy.portnumber"));
        const unsigned long port = wcstoul(options.port.c_str(), nullptr, 10);
        if (!port || port > 65535) return Fail(ERROR_INVALID_PARAMETER, l10n::Text("error.proxy.portrange"));
        Append(arguments, QuoteArgument(L"--proxy-server=" + options.protocol + L"://" + options.host + L":" + options.port));
    }
    if (options.silent) Append(arguments, L"--no-startup-window");
    if (!options.bypass.empty()) Append(arguments, QuoteArgument(L"--proxy-bypass-list=" + options.bypass));
    Append(arguments, options.extra);
    if (arguments.size() > MaxArguments) return Fail(ERROR_FILENAME_EXCED_RANGE, l10n::Text("error.arguments.length"));
    std::vector<Token> tokens;
    Result result = Tokens(arguments, tokens);
    if (!result) return result;
    unsigned counts[ManagedOptionCount]{};
    for (const auto& token : tokens) {
        if (token.value == L"--") break;
        const ManagedOption kind = ManagedKind(token.value);
        if (kind != Other && ++counts[kind] > 1)
            return Fail(ERROR_INVALID_PARAMETER, l10n::Text("error.arguments.duplicate"));
        if (token.value == L"--launcher-check" || token.value == L"--launcher-info" || token.value == L"--launcher-chain")
            return Fail(ERROR_INVALID_PARAMETER, l10n::Text("error.arguments.reserved"));
    }
    return {};
}
Result ParseArguments(const std::wstring& arguments, Options& options) {
    options.proxy = false; options.silent = false; options.bypass.clear(); options.extra.clear();
    std::vector<Token> tokens;
    Result result = Tokens(arguments, tokens);
    if (!result) { options.extra = arguments; return result; }
    unsigned counts[ManagedOptionCount]{};
    for (const auto& token : tokens) {
        if (token.value == L"--") break;
        ++counts[ManagedKind(token.value)];
    }
    // Preserve the complete original text when duplicate managed options cannot be represented by the form.
    if (counts[ProxyServer] > 1 || counts[SilentStart] > 1 || counts[ProxyBypass] > 1) {
        options.extra = arguments;
        return {};
    }
    size_t lastUnknownEnd = 0;
    bool previousUnknown = false, positional = false;
    for (const auto& token : tokens) {
        if (token.value == L"--") positional = true;
        if (!positional && token.value == L"--no-startup-window") { options.silent = true; previousUnknown = false; continue; }
        if (!positional && token.value.rfind(L"--proxy-bypass-list=", 0) == 0 && token.value.size() > 20) {
            options.bypass = token.value.substr(20); previousUnknown = false; continue;
        }
        if (!positional && token.value.rfind(L"--proxy-server=", 0) == 0) {
            const std::wstring value = token.value.substr(15);
            const size_t separator = value.find(L"://"), colon = value.rfind(L':');
            if (separator != std::wstring::npos && colon > separator + 3) {
                Options candidate = options;
                candidate.proxy = true; candidate.protocol = value.substr(0, separator);
                candidate.host = value.substr(separator + 3, colon - separator - 3); candidate.port = value.substr(colon + 1);
                candidate.extra.clear();
                std::wstring ignored;
                if (BuildArguments(candidate, ignored)) {
                    options.proxy = true; options.protocol = candidate.protocol;
                    options.host = candidate.host; options.port = candidate.port;
                    previousUnknown = false;
                    continue;
                }
            }
        }
        if (previousUnknown) options.extra += arguments.substr(lastUnknownEnd, token.end - lastUnknownEnd);
        else Append(options.extra, token.raw);
        previousUnknown = true;
        lastUnknownEnd = token.end;
    }
    return {};
}
} // namespace setup
