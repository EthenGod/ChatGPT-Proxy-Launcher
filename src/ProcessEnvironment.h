// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "AutomationIO.h"
#include <algorithm>
#include <map>
#include <string_view>

namespace native {
using EnvironmentName = std::wstring_view (*)(std::wstring_view);
inline std::vector<wchar_t> Environment(const std::map<std::wstring, std::wstring>& values,
                                        EnvironmentName normalizeName = nullptr) {
    struct CaseLess {
        bool operator()(std::wstring_view a, std::wstring_view b) const {
            return CompareStringOrdinal(a.data(), static_cast<int>(a.size()),
                b.data(), static_cast<int>(b.size()), TRUE) == CSTR_LESS_THAN;
        }
    };
    LPWCH raw = GetEnvironmentStringsW();
    if (!raw) throw automation::IoError(GetLastError());
    struct Env { LPWCH p; ~Env() { FreeEnvironmentStringsW(p); } } env{raw};
    // The environment snapshot and caller's overrides outlive these views.
    std::map<std::wstring_view, std::wstring_view, CaseLess> items;
    for (PCWSTR p = raw; *p;) {
        const std::wstring_view item(p);
        p += item.size() + 1;
        const auto sep = item.find(L'=', 1); // Keep per-drive entries such as =C:=C:\dir.
        if (sep == item.npos) continue;
        const auto key = item.substr(0, sep);
        // Inherited blocks are usually sorted. An out-of-order or duplicate key still uses ordinal lookup.
        items.insert_or_assign(items.end(), normalizeName ? normalizeName(key) : key, item.substr(sep + 1));
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
