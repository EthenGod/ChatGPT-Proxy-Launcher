// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <ws2tcpip.h>
#include <string_view>

#pragma comment(lib, "ws2_32.lib")

namespace cloudproxy {
inline constexpr wchar_t OffSwitch[] = L"--launcher-no-cloud-proxy";
inline constexpr std::wstring_view ServerPrefix = L"--proxy-server=";

// Chromium trims Unicode White_Space from each decoded Windows argument.
inline bool ArgumentSpace(wchar_t c) noexcept {
    return (c >= L'\t' && c <= L'\r') || c == L' ' || c == 0x85 || c == 0xa0 ||
        c == 0x1680 || (c >= 0x2000 && c <= 0x200a) || c == 0x2028 || c == 0x2029 ||
        c == 0x202f || c == 0x205f || c == 0x3000;
}

inline std::wstring_view TrimArgument(std::wstring_view arg) noexcept {
    while (!arg.empty() && ArgumentSpace(arg.front())) arg.remove_prefix(1);
    while (!arg.empty() && ArgumentSpace(arg.back())) arg.remove_suffix(1);
    return arg;
}

inline bool IpAddress(std::wstring_view host, int family) noexcept {
    wchar_t literal[INET6_ADDRSTRLEN]{};
    if (host.empty() || host.size() >= _countof(literal) || host.find(L'\0') != host.npos) return false;
    host.copy(literal, host.size());
    IN6_ADDR address{};
    return InetPtonW(family, literal, &address) == 1;
}

// Only a single host and a decimal port. Never copy userinfo, paths or an encoded authority.
inline bool ValidEndpoint(std::wstring_view url) noexcept {
    if (url.substr(0, 7) == L"http://") url.remove_prefix(7);
    else if (url.substr(0, 8) == L"https://") url.remove_prefix(8);
    else return false;
    const auto colon = url.rfind(L':');
    if (colon == url.npos) return false;
    auto host = url.substr(0, colon);
    const auto port = url.substr(colon + 1);
    if (host.empty() || port.empty() || port.size() > 5) return false;
    unsigned number = 0;
    for (const auto digit : port) {
        if (digit < L'0' || digit > L'9') return false;
        number = number * 10 + (digit - L'0');
    }
    if (number == 0 || number > 65535) return false;
    if (host.front() == L'[' && host.back() == L']')
        return IpAddress(host.substr(1, host.size() - 2), AF_INET6);
    if (host.back() == L'.') host.remove_suffix(1);
    if (host.empty() || host.size() > 253) return false;
    const auto last = host.substr(host.rfind(L'.') + 1);
    // URL parsers treat a numeric final label (including 0x...) as IPv4.
    const bool hexNumber = (last.substr(0, 2) == L"0x" || last.substr(0, 2) == L"0X") &&
        last.substr(2).find_first_not_of(L"0123456789abcdefABCDEF") == last.npos;
    if (hexNumber || last.find_first_not_of(L"0123456789") == last.npos)
        return IpAddress(host, AF_INET);
    while (!host.empty()) {
        const auto dot = host.find(L'.');
        const auto label = host.substr(0, dot);
        if (label.empty() || label.size() > 63 || label.front() == L'-' || label.back() == L'-') return false;
        for (const auto c : label) {
            if (!(c >= L'a' && c <= L'z') && !(c >= L'A' && c <= L'Z') &&
                !(c >= L'0' && c <= L'9') && c != L'-') return false;
        }
        if (dot == host.npos) break;
        host.remove_prefix(dot + 1);
    }
    return true;
}

inline bool ConflictingSwitch(std::wstring_view arg) noexcept {
    if (arg.substr(0, 2) == L"--") arg.remove_prefix(2);
    else if (!arg.empty() && (arg.front() == L'-' || arg.front() == L'/')) arg.remove_prefix(1);
    else return false;
    const auto equals = arg.find(L'=');
    const auto key = arg.substr(0, equals);
    const auto matches = [&](PCWSTR name) {
        return CompareStringOrdinal(key.data(), static_cast<int>(key.size()), name, -1, TRUE) == CSTR_EQUAL;
    };
    // Chromium bypass rules and Node NO_PROXY do not share a matching language.
    if (matches(L"proxy-bypass-list")) return equals != arg.npos && equals + 1 < arg.size();
    for (const auto name : {L"proxy-server", L"no-proxy-server", L"proxy-pac-url",
                            L"proxy-auto-detect", OffSwitch + 2}) {
        if (matches(name)) return true;
    }
    return false;
}
} // namespace cloudproxy
