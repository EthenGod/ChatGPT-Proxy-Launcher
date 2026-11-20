// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "AutomationIO.h"
#include <algorithm>
#include <map>

namespace native {
inline std::vector<wchar_t> Environment(const std::map<std::wstring, std::wstring>& values) {
    struct CaseLess {
        bool operator()(const std::wstring& a, const std::wstring& b) const {
            return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_LESS_THAN;
        }
    };
    LPWCH raw = GetEnvironmentStringsW();
    if (!raw) throw automation::IoError(GetLastError());
    struct Env { LPWCH p; ~Env() { FreeEnvironmentStringsW(p); } } env{raw};
    std::map<std::wstring, std::wstring, CaseLess> items;
    for (PCWSTR p = raw; *p; p += wcslen(p) + 1) {
        const std::wstring item(p);
        const auto sep = item.find(L'=', 1); // Keep per-drive entries such as =C:=C:\dir.
        if (sep != item.npos) items[item.substr(0, sep)] = item.substr(sep + 1);
    }
    for (const auto& v : values) items[v.first] = v.second;
    size_t size = 1;
    for (const auto& item : items) size += item.first.size() + item.second.size() + 2;
    std::vector<wchar_t> out;
    out.reserve(std::max(size, size_t{2}));
    for (const auto& item : items) {
        out.insert(out.end(), item.first.begin(), item.first.end());
        out.push_back(L'=');
        out.insert(out.end(), item.second.begin(), item.second.end());
        out.push_back(0);
    }
    if (out.empty()) out.push_back(0);
    out.push_back(0);
    return out;
}
} // namespace native
