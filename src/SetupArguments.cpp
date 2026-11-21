// SPDX-License-Identifier: GPL-3.0-only
#include "SetupCore.h"
#include "CloudProxy.h"
#include "Localization.h"
#include "AutomationIO.h"
#include "NativeMemory.h"
#include <shellapi.h>
#include <cwchar>
#include <optional>
#include <string_view>

namespace setup {
namespace {
Result Fail(DWORD code, const std::wstring& what) {
    return { code ? code : ERROR_GEN_FAILURE, what + L"\n" + ErrorText(code) };
}
// raw borrows the caller's command line for the duration of BuildArguments or ParseArguments.
struct Token {
    std::wstring_view raw;
    size_t end;
    std::optional<std::wstring> decoded;

    std::wstring_view Value() const { return decoded ? std::wstring_view(*decoded) : raw; }
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
        bool quoted = false, hasQuote = false;
        size_t slashes = 0;
        for (; i < arguments.size(); ++i) {
            const wchar_t c = arguments[i];
            if (!quoted && (c == L' ' || c == L'\t')) break;
            if (c == L'"') {
                hasQuote = true;
                if (slashes % 2 == 0) quoted = !quoted;
            }
            slashes = c == L'\\' ? slashes + 1 : 0;
        }
        if (quoted) return Fail(ERROR_INVALID_PARAMETER, l10n::Text("error.arguments.quote"));
        Token token{std::wstring_view(arguments).substr(start, i - start), i, std::nullopt};
        // Only quotes change the value of a Windows argument, including escaped quotes.
        if (hasQuote) {
            std::wstring command = L"program ";
            command.append(token.raw);
            int count = 0;
            native::LocalPtr<LPWSTR> argv(CommandLineToArgvW(command.c_str(), &count));
            if (!argv) return Fail(GetLastError(), l10n::Text("error.arguments.parse"));
            if (count != 2) return Fail(ERROR_INVALID_DATA, l10n::Text("error.arguments.format"));
            token.decoded.emplace(argv.get()[1]);
        }
        tokens.push_back(std::move(token));
    }
    return {};
}
enum ManagedOption { Other, ProxyServer, SilentStart, ProxyBypass, ManagedOptionCount };
ManagedOption ManagedKind(std::wstring_view arg) {
    const auto key = arg.substr(0, arg.find(L'='));
    if (key == L"--proxy-server") return ProxyServer;
    if (key == L"--no-startup-window") return SilentStart;
    if (key == L"--proxy-bypass-list") return ProxyBypass;
    return Other;
}
void Append(std::wstring& text, std::wstring_view item) {
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
    if (!options.cloudCodex) Append(arguments, cloudproxy::OffSwitch);
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
    for (size_t i = options.cloudCodex ? 0 : 1; i < tokens.size(); ++i) {
        const auto value = tokens[i].Value();
        if (value == L"--") break;
        const ManagedOption kind = ManagedKind(value);
        if (kind != Other && ++counts[kind] > 1)
            return Fail(ERROR_INVALID_PARAMETER, l10n::Text("error.arguments.duplicate"));
        if (value == cloudproxy::OffSwitch || value == L"--launcher-check" || value == L"--launcher-info" || value == L"--launcher-chain")
            return Fail(ERROR_INVALID_PARAMETER, l10n::Text("error.arguments.reserved"));
    }
    return {};
}
Result ParseArguments(const std::wstring& arguments, Options& options) {
    options.proxy = false; options.cloudCodex = true; options.silent = false; options.bypass.clear(); options.extra.clear();
    std::vector<Token> tokens;
    Result result = Tokens(arguments, tokens);
    if (!result) { options.extra = arguments; return result; }
    options.cloudCodex = tokens.empty() || tokens.front().Value() != cloudproxy::OffSwitch;
    unsigned counts[ManagedOptionCount]{};
    for (const auto& token : tokens) {
        const auto value = token.Value();
        if (value == L"--") break;
        ++counts[ManagedKind(value)];
    }
    // Preserve the complete original text when duplicate managed options cannot be represented by the form.
    if (counts[ProxyServer] > 1 || counts[SilentStart] > 1 || counts[ProxyBypass] > 1) {
        options.extra = arguments;
        return {};
    }
    size_t lastUnknownEnd = 0;
    bool previousUnknown = false, positional = false;
    for (size_t i = options.cloudCodex ? 0 : 1; i < tokens.size(); ++i) {
        const auto& token = tokens[i];
        const auto argument = token.Value();
        if (argument == L"--") positional = true;
        if (!positional && argument == L"--no-startup-window") { options.silent = true; previousUnknown = false; continue; }
        if (!positional && argument.rfind(L"--proxy-bypass-list=", 0) == 0 && argument.size() > 20) {
            options.bypass = argument.substr(20); previousUnknown = false; continue;
        }
        if (!positional && argument.rfind(L"--proxy-server=", 0) == 0) {
            const auto value = argument.substr(15);
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
        if (previousUnknown) options.extra.append(arguments, lastUnknownEnd, token.end - lastUnknownEnd);
        else Append(options.extra, token.raw);
        previousUnknown = true;
        lastUnknownEnd = token.end;
    }
    return {};
}
} // namespace setup
