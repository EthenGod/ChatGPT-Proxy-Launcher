// SPDX-License-Identifier: GPL-3.0-only
#include "../src/SetupCore.h"
#include <shlobj.h>
#include <stdio.h>
#include <type_traits>
#include "MemoryFault.h"

static int taskAllocations = 0, localAllocations = 0;
static HRESULT WINAPI TestKnownFolder(REFKNOWNFOLDERID, DWORD, HANDLE, PWSTR* path) {
    constexpr wchar_t fake[] = L"C:\\Synthetic long folder for allocation failure tests";
    *path = static_cast<PWSTR>(CoTaskMemAlloc(sizeof(fake)));
    if (!*path) return E_OUTOFMEMORY;
    CopyMemory(*path, fake, sizeof(fake)); ++taskAllocations;
    return S_OK;
}
static void WINAPI TestTaskFree(void* value) {
    if (value) --taskAllocations;
    CoTaskMemFree(value);
}
static HLOCAL WINAPI TestLocalFree(HLOCAL value) {
    if (value) --localAllocations;
    return LocalFree(value);
}
static DWORD WINAPI TestFormat(DWORD, LPCVOID, DWORD, DWORD, LPWSTR output, DWORD, va_list*) {
    constexpr wchar_t text[] = L"Synthetic Windows error message for allocation testing\r\n";
    auto value = static_cast<PWSTR>(LocalAlloc(LMEM_FIXED, sizeof(text)));
    if (!value) return 0;
    CopyMemory(value, text, sizeof(text));
    *reinterpret_cast<PWSTR*>(output) = value; ++localAllocations;
    return _countof(text) - 1;
}
#define SHGetKnownFolderPath TestKnownFolder
#define CoTaskMemFree TestTaskFree
#define LocalFree TestLocalFree
#define FormatMessageW TestFormat
// Keep instrumented deleters distinct from production inline definitions in linked objects.
#define native memory_test_native
#include "../src/SetupCore.cpp"
#undef native
#undef SHGetKnownFolderPath
#undef CoTaskMemFree
#undef LocalFree
#undef FormatMessageW

#define REQUIRE(x) do { if (!(x)) { printf("FAIL memory line %d: %s\n", __LINE__, #x); return 20; } } while (0)
static_assert(!std::is_copy_constructible_v<memory_test_native::TaskPtr<wchar_t>>);
static_assert(!std::is_copy_constructible_v<memory_test_native::LocalPtr<wchar_t>>);
int main() {
    unsigned failedPaths = 0, succeededPaths = 0, failedErrors = 0;
    for (int fail = 0; fail < 40; ++fail) {
        setup::Paths paths;
        try { memory_fault::Scope fault(fail); setup::GetPaths(paths); ++succeededPaths; }
        catch (const std::bad_alloc&) { ++failedPaths; }
        REQUIRE(taskAllocations == 0);
    }
    for (int fail = 0; fail < 16; ++fail) {
        try { memory_fault::Scope fault(fail); setup::ErrorText(ERROR_ACCESS_DENIED); }
        catch (const std::bad_alloc&) { ++failedErrors; }
        REQUIRE(localAllocations == 0);
    }
    REQUIRE(failedPaths && succeededPaths && failedErrors);
    printf("PASS system memory ownership: 56 allocation-failure positions, zero outstanding API buffers, no user paths accessed\n");
    // Reading existing fields must still work when no further heap allocation is possible.
    auto value = automation::Json::Object();
    constexpr wchar_t key[] = L"long-existing-json-field";
    value[key] = automation::Json(true);
    size_t lookupAllocations = 0;
    bool lookupCorrect = true;
    {
        memory_fault::Scope count;
        for (unsigned i = 0; i < 1000; ++i) {
            lookupCorrect = lookupCorrect && value.Has(key) && !value.Has(L"long-missing-json-field")
                && value.At(key).Bool() && value[key].Bool();
        }
        lookupAllocations = memory_fault::calls;
    }
    printf("JSON field lookup: %zu allocations in 1000 rounds\n", lookupAllocations);
    REQUIRE(lookupCorrect && lookupAllocations == 0 && value.object.size() == 1);
    {
        memory_fault::Scope fault(0);
        REQUIRE(value.Has(key) && !value.Has(L"missing-field") && value.At(key).Bool() && value[key].Bool());
    }
    bool missing = false;
    try { value.At(L"long-missing-json-field"); } catch (const automation::Invalid&) { missing = true; }
    REQUIRE(missing && value.object.size() == 1);
    value[L"new-field"] = automation::Json(false);
    REQUIRE(!value.At(L"new-field").Bool() && value.object.size() == 2);
    const auto roundTrip = automation::ParseJson(automation::WriteJson(value));
    REQUIRE(roundTrip.At(key).Bool() && !roundTrip.At(L"new-field").Bool());
    const std::wstring text(200, L'x');
    size_t literalAllocations = 0, stringAllocations = 0;
    {
        memory_fault::Scope count;
        const automation::Json literal(text.c_str());
        REQUIRE(literal.String() == text);
        literalAllocations = memory_fault::calls;
    }
    {
        memory_fault::Scope count;
        const automation::Json string(text);
        REQUIRE(string.String() == text);
        stringAllocations = memory_fault::calls;
    }
    REQUIRE(literalAllocations == stringAllocations);
    puts("PASS JSON lookup: no temporary allocations, missing fields unchanged and insertion/round trip preserved");
    const std::wstring input = L"C:\\Synthetic long folder\\configuration\\..\\中文 path\\discovery.json";
    const std::wstring expected = L"C:\\Synthetic long folder\\中文 path\\discovery.json";
    REQUIRE(automation::FullPath(input) == expected);
    size_t pathAllocations = 0, pathBytes = 0;
    bool pathsCorrect = true;
    constexpr unsigned rounds = 20000;
    LARGE_INTEGER frequency{}, start{}, finish{};
    QueryPerformanceFrequency(&frequency); QueryPerformanceCounter(&start);
    {
        memory_fault::Scope count;
        for (unsigned i = 0; i < rounds; ++i) pathsCorrect = pathsCorrect && automation::FullPath(input) == expected;
        pathAllocations = memory_fault::calls; pathBytes = memory_fault::bytes;
    }
    QueryPerformanceCounter(&finish);
    printf("Path normalization: %.2f allocations, %.0f requested bytes, %.3f us per call (%u synthetic rounds)\n",
        static_cast<double>(pathAllocations) / rounds, static_cast<double>(pathBytes) / rounds,
        static_cast<double>(finish.QuadPart - start.QuadPart) * 1000000 / frequency.QuadPart / rounds, rounds);
    REQUIRE(automation::FullPath(L"C:\\") == L"C:\\");
    REQUIRE(automation::FullPath(L"C:\\one\\..\\two\\.\\") == L"C:\\two");
    for (const size_t length : {size_t(259), size_t(260), size_t(261), size_t(4096), size_t(32766)}) {
        const std::wstring longPath = L"C:\\" + std::wstring(length - 3, L'x');
        REQUIRE(automation::FullPath(longPath) == longPath);
    }
    for (const auto& invalid : {L"relative\\file", L"C:file", L"C:\\con.txt", L"C:\\a\\Nul", L"C:\\Com1.log",
        L"C:\\lPt9", L"C:\\COM\u00b9.txt", L"C:\\LPT\u00b2", L"C:\\COM\u00b3", L"C:\\file:stream", L"C:\\a*", L"\\\\server\\share"}) {
        bool rejected = false;
        try { automation::FullPath(invalid); } catch (const automation::Invalid&) { rejected = true; }
        REQUIRE(rejected);
    }
    bool rejected = false;
    try { automation::FullPath(L"C:\\" + std::wstring(32764, L'x')); } catch (const automation::Invalid&) { rejected = true; }
    REQUIRE(rejected);
    REQUIRE(automation::FullPath(L"C:\\COM10.txt") == L"C:\\COM10.txt");
    REQUIRE(pathsCorrect && pathBytes / rounds < 4096);
    puts("PASS path normalization: short/long buffer boundaries, Unicode, parent segments and reserved/invalid names");
    return 0;
}
