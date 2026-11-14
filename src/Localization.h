// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>

namespace l10n {
inline constexpr wchar_t PreferenceKey[] = L"Software\\Community\\ChatGPTLauncherSetup";
struct Language { const wchar_t* tag; const wchar_t* name; const wchar_t* font; };
struct Argument { std::string_view name; std::wstring_view value; };
size_t Count() noexcept;
const Language& At(size_t index) noexcept;
const Language& Current() noexcept;
bool Select(std::wstring_view tag) noexcept;
// Returns Count() when no catalog matches. Exact tags take priority over the language family.
size_t Match(std::wstring_view tag) noexcept;
bool FollowsSystem() noexcept;
void UseSystem() noexcept;
// Read the preference once. Call only at a user-facing entry point, never while painting.
void Initialize() noexcept;
const wchar_t* Text(std::string_view key) noexcept;
std::wstring Format(std::string_view key, std::initializer_list<Argument> arguments);
unsigned long SystemLanguage() noexcept;
}
