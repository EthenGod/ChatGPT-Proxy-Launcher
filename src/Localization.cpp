// SPDX-License-Identifier: GPL-3.0-only
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <atomic>
#include "Localization.h"
#include "../work/LocalizationData.h"

namespace l10n {
namespace {
std::atomic<size_t> selected{0};
std::atomic<bool> initialized{false};
std::atomic<bool> followsSystem{true};
bool Same(std::wstring_view first, std::wstring_view second) noexcept {
    return first.size() == second.size() && CompareStringOrdinal(first.data(), static_cast<int>(first.size()),
        second.data(), static_cast<int>(second.size()), TRUE) == CSTR_EQUAL;
}
}
size_t Count() noexcept { return std::size(data::Languages); }
const Language& At(size_t index) noexcept { return data::Languages[index < Count() ? index : data::Fallback].language; }
const Language& Current() noexcept { return At(selected.load(std::memory_order_relaxed)); }
bool Select(std::wstring_view tag) noexcept {
    for (size_t i = 0; i < Count(); ++i) if (Same(tag, At(i).tag)) {
        selected.store(i, std::memory_order_relaxed); followsSystem.store(false, std::memory_order_relaxed);
        initialized.store(true, std::memory_order_relaxed); return true;
    }
    return false;
}
size_t Match(std::wstring_view tag) noexcept {
    if (tag.empty() || tag.size() >= LOCALE_NAME_MAX_LENGTH) return Count();
    for (size_t i = 0; i < Count(); ++i) if (Same(tag, At(i).tag)) return i;
    const auto primary = tag.substr(0, tag.find(L'-'));
    for (size_t i = 0; i < Count(); ++i) {
        const std::wstring_view available(At(i).tag);
        if (Same(primary, available.substr(0, available.find(L'-')))) return i;
    }
    return Count();
}
bool FollowsSystem() noexcept { return followsSystem.load(std::memory_order_relaxed); }
void UseSystem() noexcept {
    size_t match = Count();
    // A fixed bound keeps failure handling allocation-free, even with a damaged user profile.
    wchar_t languages[4096]{}; ULONG count = 0, size = static_cast<ULONG>(std::size(languages));
    if (GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &count, languages, &size)) {
        for (PCWSTR tag = languages; *tag && match == Count(); tag += wcslen(tag) + 1) match = Match(tag);
    }
    wchar_t tag[LOCALE_NAME_MAX_LENGTH]{};
    if (match == Count() && LCIDToLocaleName(GetUserDefaultUILanguage(), tag, LOCALE_NAME_MAX_LENGTH, 0)) match = Match(tag);
    selected.store(match < Count() ? match : data::Fallback, std::memory_order_relaxed);
    followsSystem.store(true, std::memory_order_relaxed);
    initialized.store(true, std::memory_order_relaxed);
}
void Initialize() noexcept {
    if (initialized.load(std::memory_order_relaxed)) return;
    wchar_t tag[LOCALE_NAME_MAX_LENGTH]{}; DWORD bytes = sizeof(tag);
    if (RegGetValueW(HKEY_CURRENT_USER, PreferenceKey, L"Language", RRF_RT_REG_SZ, nullptr, tag, &bytes) == ERROR_SUCCESS
        && tag[0] && Select(tag)) return;
    UseSystem();
}
const wchar_t* Text(std::string_view key) noexcept {
    const auto found = std::lower_bound(std::begin(data::Keys), std::end(data::Keys), key,
        [](const char* entry, std::string_view value) { return std::string_view(entry) < value; });
    if (found == std::end(data::Keys) || key != *found) return L"[missing translation]";
    return data::Languages[selected.load(std::memory_order_relaxed)].text[found - std::begin(data::Keys)];
}
std::wstring Format(std::string_view key, std::initializer_list<Argument> arguments) {
    const std::wstring_view pattern(Text(key));
    std::wstring result; result.reserve(pattern.size());
    for (size_t position = 0; position < pattern.size();) {
        const auto start = pattern.find(L'{', position);
        if (start == pattern.npos) { result.append(pattern.substr(position)); break; }
        result.append(pattern.substr(position, start - position));
        const auto end = pattern.find(L'}', start + 1);
        if (end == pattern.npos) { result.append(pattern.substr(start)); break; }
        const auto name = pattern.substr(start + 1, end - start - 1);
        bool replaced = false;
        for (const auto& argument : arguments) if (name.size() == argument.name.size()
            && std::equal(name.begin(), name.end(), argument.name.begin())) {
            result.append(argument.value); replaced = true; break;
        }
        if (!replaced) result.append(pattern.substr(start, end - start + 1));
        position = end + 1;
    }
    return result;
}
unsigned long SystemLanguage() noexcept {
    const auto locale = LocaleNameToLCID(Current().tag, 0);
    return locale ? LANGIDFROMLCID(locale) : MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
}
}
