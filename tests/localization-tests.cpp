// SPDX-License-Identifier: GPL-3.0-only
#include "../src/Localization.h"
#include "MemoryFault.h"
#include <cwchar>
#include <cstdio>
#include <chrono>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
static PCWSTR preference = L"en-US", preferred = L"es-ES\0ja-JP\0";
static LANGID systemLanguage = 0x40a;
static unsigned preferenceReads = 0;
static BOOL WINAPI TestUILanguages(DWORD, PULONG count, PZZWSTR buffer, PULONG size) {
    size_t length = 1; for (PCWSTR p = preferred; *p; p += wcslen(p) + 1) length += wcslen(p) + 1;
    if (*size < length) return FALSE;
    memcpy(buffer, preferred, length * sizeof(wchar_t)); *size = static_cast<ULONG>(length); *count = 2; return TRUE;
}
static LANGID WINAPI TestUILanguage() { return systemLanguage; }
static LSTATUS WINAPI TestPreference(HKEY, PCWSTR, PCWSTR, DWORD, LPDWORD, PVOID value, LPDWORD size) {
    ++preferenceReads;
    if (*size < (wcslen(preference) + 1) * sizeof(wchar_t)) return ERROR_MORE_DATA;
    wcscpy_s(static_cast<PWSTR>(value), *size / sizeof(wchar_t), preference); return ERROR_SUCCESS;
}
#define GetUserPreferredUILanguages TestUILanguages
#define GetUserDefaultUILanguage TestUILanguage
#define RegGetValueW TestPreference
#include "../src/Localization.cpp"
#undef RegGetValueW
#undef GetUserDefaultUILanguage
#undef GetUserPreferredUILanguages
#define REQUIRE(test) do { if (!(test)) { printf("FAIL localization line %d: %s\n", __LINE__, #test); return 1; } } while (0)
int main() {
    REQUIRE(l10n::Count() >= 3 && !wcscmp(l10n::Current().tag, L"zh-CN"));
    l10n::Initialize(); REQUIRE(!wcscmp(l10n::Current().tag, L"en-US") && !l10n::FollowsSystem());
    l10n::Initialize(); REQUIRE(preferenceReads == 1);
    l10n::UseSystem(); REQUIRE(!wcscmp(l10n::Current().tag, L"ja-JP") && l10n::FollowsSystem());
    preferred = L"es-ES\0de-DE\0"; l10n::UseSystem(); REQUIRE(!wcscmp(l10n::Current().tag, L"zh-CN"));
    preferred = L"\0"; systemLanguage = 0x409; l10n::UseSystem(); REQUIRE(!wcscmp(l10n::Current().tag, L"en-US"));
    l10n::Select(L"zh-CN");
    REQUIRE(l10n::Match(L"EN-us") == 1 && l10n::Match(L"en-GB") == 1);
    REQUIRE(l10n::Match(L"ja") == 2 && l10n::Match(L"zh-TW") == 0);
    REQUIRE(l10n::Match(L"xx-YY") == l10n::Count() && l10n::Match(L"") == l10n::Count());
    REQUIRE(!l10n::Select(L"unknown") && !wcscmp(l10n::Current().tag, L"zh-CN"));
    const auto chinese = l10n::Text("common.close");
    REQUIRE(l10n::Select(L"en-US") && !l10n::FollowsSystem() && !wcscmp(l10n::Text("common.close"), L"Close"));
    REQUIRE(!wcscmp(chinese, L"关闭")); // Tooltip pointers remain valid after switching.
    REQUIRE(l10n::Format("error.system.details", {{"code", L"5"}, {"message", L"Literal {code}"}}) == L"Literal {code} (error code 5)");
    REQUIRE(l10n::Format("setup.version", {}) == L"Setup v{version}");
    const auto begin = std::chrono::steady_clock::now();
    size_t allocationCount = 0;
    {
        memory_fault::Scope failAll(0);
        for (int i = 0; i < 100000; ++i) {
            REQUIRE(l10n::Text("setup.option.proxy")[0] == L'U');
            REQUIRE(l10n::Match(L"ja-JP") == 2);
        }
        allocationCount = memory_fault::calls;
    }
    REQUIRE(allocationCount == 0);
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - begin).count();
    REQUIRE(l10n::Select(L"ja-JP") && l10n::SystemLanguage() == 0x411);
    REQUIRE(l10n::Select(L"zh-CN") && l10n::SystemLanguage() == 0x804);
    printf("PASS localization: regional matching, immutable pointer lifetime, literal placeholders and allocation-free lookup/matching (100000 rounds: %lld us)\n", elapsed);
}
