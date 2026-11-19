// SPDX-License-Identifier: GPL-3.0-only
#include "../src/AutomationIO.h"
#include "MemoryFault.h"
#include <stdio.h>

static std::wstring probePath, renamePath;
static bool probe = false, writeBlocked = false, renameBlocked = false, failRemoval = false, shortRead = false;
static unsigned dispositionCalls = 0;
static DWORD fileIdError = ERROR_SUCCESS;
static bool omitLegacyId = false;
static BOOL WINAPI TestFileInformation(HANDLE file, LPBY_HANDLE_FILE_INFORMATION info) {
    const auto ok = GetFileInformationByHandle(file, info);
    if (ok && omitLegacyId) { info->nFileIndexHigh = 0; info->nFileIndexLow = 0; }
    return ok;
}
static BOOL WINAPI TestGetInformationEx(HANDLE file, FILE_INFO_BY_HANDLE_CLASS kind, LPVOID data, DWORD bytes) {
    if (kind == FileIdInfo && fileIdError) { SetLastError(fileIdError); return FALSE; }
    return GetFileInformationByHandleEx(file, kind, data, bytes);
}
static BOOL WINAPI TestSetInformation(HANDLE file, FILE_INFO_BY_HANDLE_CLASS kind, LPVOID data, DWORD bytes) {
    if (kind == FileDispositionInfo) {
        ++dispositionCalls;
        if (probe) {
            HANDLE writer = CreateFileW(probePath.c_str(), GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
            writeBlocked = writer == INVALID_HANDLE_VALUE && GetLastError() == ERROR_SHARING_VIOLATION;
            if (writer != INVALID_HANDLE_VALUE) CloseHandle(writer);
            renameBlocked = !MoveFileExW(probePath.c_str(), renamePath.c_str(), 0)
                && GetLastError() == ERROR_SHARING_VIOLATION;
        }
        if (failRemoval) { SetLastError(ERROR_ACCESS_DENIED); return FALSE; }
    }
    return SetFileInformationByHandle(file, kind, data, bytes);
}
static BOOL WINAPI TestRead(HANDLE file, LPVOID data, DWORD count, LPDWORD got, LPOVERLAPPED overlapped) {
    return ReadFile(file, data, shortRead && count ? count - 1 : count, got, overlapped);
}
#define SetFileInformationByHandle TestSetInformation
#define ReadFile TestRead
#define GetFileInformationByHandleEx TestGetInformationEx
#define GetFileInformationByHandle TestFileInformation
#include "../src/AutomationIO.cpp"
#undef GetFileInformationByHandle
#undef GetFileInformationByHandleEx
#undef SetFileInformationByHandle
#undef ReadFile

#define REQUIRE(x) do { if (!(x)) { printf("FAIL file line %d: %s\n", __LINE__, #x); return 20; } } while (0)
static void Write(const std::wstring& path, const std::string& value) {
    automation::Handle file(CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, 0, nullptr));
    if (!file) throw automation::IoError(GetLastError());
    automation::WriteBytes(file.value, value);
}
static DWORD Remove(const std::wstring& path, std::string_view expected) {
    try { automation::RemoveUnchangedFile(path, expected); return 0; }
    catch (const automation::IoError& error) { return error.code; }
}
int wmain(int argc, wchar_t** argv) {
    if (argc != 2) return 2;
    try {
        const auto root = std::wstring(argv[1]) + L"\\files-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64());
        REQUIRE(CreateDirectoryW(root.c_str(), nullptr));
        auto alias = root;
        alias.replace(alias.rfind(L'\\') + 1, 5, L"FILES");
        if (GetFileAttributesW(alias.c_str()) != INVALID_FILE_ATTRIBUTES) {
            const auto other = root + L"-other";
            REQUIRE(CreateDirectoryW(other.c_str(), nullptr));
            fileIdError = ERROR_NOT_SUPPORTED;
            for (const bool pathOnly : {false, true}) {
                omitLegacyId = pathOnly;
                automation::CheckPathUnderRoot(alias + L"\\child.exe", root);
                REQUIRE(!automation::SameRootDirectory(root, other));
            }
            omitLegacyId = false;
            fileIdError = ERROR_ACCESS_DENIED;
            bool refused = false;
            try { automation::CheckPathUnderRoot(alias + L"\\child.exe", root); } catch (const automation::Invalid&) { refused = true; }
            REQUIRE(refused);
            fileIdError = ERROR_SUCCESS;
        }
        const auto changed = root + L"\\changed.json"; Write(changed, "foreign");
        REQUIRE(Remove(changed, "managed") == ERROR_RETRY && dispositionCalls == 0);
        REQUIRE(automation::ReadBytes(changed) == "foreign");
        REQUIRE(Remove(changed, "different size") == ERROR_RETRY && dispositionCalls == 0);
        const auto locked = root + L"\\locked.json"; Write(locked, "managed");
        {
            automation::Handle reader(CreateFileW(locked.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr));
            REQUIRE(reader && Remove(locked, "managed") == ERROR_SHARING_VIOLATION);
        }
        REQUIRE(automation::ReadBytes(locked) == "managed");
        shortRead = true; const DWORD readError = Remove(locked, "managed"); shortRead = false;
        REQUIRE(readError == ERROR_READ_FAULT && automation::ReadBytes(locked) == "managed");
        failRemoval = true; const DWORD deleteError = Remove(locked, "managed"); failRemoval = false;
        REQUIRE(deleteError == ERROR_ACCESS_DENIED && automation::ReadBytes(locked) == "managed");

        const std::string large(1024 * 1024 + 3, 'x');
        probePath = root + L"\\verified.json"; renamePath = root + L"\\replaced.json";
        Write(probePath, large); probe = true;
        DWORD handlesBefore = 0, handlesAfter = 0;
        REQUIRE(GetProcessHandleCount(GetCurrentProcess(), &handlesBefore));
        size_t allocated = 0;
        {
            memory_fault::Scope count;
            automation::RemoveUnchangedFile(probePath, large);
            allocated = memory_fault::bytes;
        }
        probe = false;
        REQUIRE(GetProcessHandleCount(GetCurrentProcess(), &handlesAfter) && handlesBefore == handlesAfter);
        REQUIRE(writeBlocked && renameBlocked && allocated < large.size());
        REQUIRE(GetFileAttributesW(probePath.c_str()) == INVALID_FILE_ATTRIBUTES);
        REQUIRE(GetFileAttributesW(renamePath.c_str()) == INVALID_FILE_ATTRIBUTES);
        const auto empty = root + L"\\empty.json"; Write(empty, "");
        REQUIRE(Remove(empty, {}) == 0 && GetFileAttributesW(empty.c_str()) == INVALID_FILE_ATTRIBUTES);
        REQUIRE(Remove(empty, {}) == ERROR_FILE_NOT_FOUND);
        printf("PASS verified file removal: concurrent write/rename blocked, content/size mismatch, locked file, short read, delete failure, empty file and handle lifetime; 1 MiB comparison allocated %zu bytes\n", allocated);
        return 0;
    } catch (...) { puts("FAIL unexpected file test exception"); return 20; }
}
