// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "AutomationJson.h"
#include <string_view>

namespace setup { struct Paths; }

namespace automation {
struct Handle {
    HANDLE value = INVALID_HANDLE_VALUE;
    Handle() = default;
    explicit Handle(HANDLE h) : value(h) {}
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    void Reset(HANDLE h = INVALID_HANDLE_VALUE) { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); value = h; }
    explicit operator bool() const { return value && value != INVALID_HANDLE_VALUE; }
};
struct IoError { DWORD code; explicit IoError(DWORD value) : code(value ? value : ERROR_GEN_FAILURE) {} };
bool SamePath(const std::wstring& a, const std::wstring& b);
std::wstring FullPath(const std::wstring& path);
std::wstring ParentPath(const std::wstring& path);
std::wstring Quote(const std::wstring& value);
void CheckPlainPath(const std::wstring& path); // Reject existing reparse points; no writes.
void CheckPathUnderRoot(const std::wstring& path, const std::wstring& root); // Strict descendant, with the same reparse checks.
std::string ReadBytes(const std::wstring& path, size_t maximum = contract::JsonBytes);
void WriteBytes(HANDLE file, const std::string& bytes);
DWORD CurrentUserSid(std::wstring& user);
// On success or ERROR_ALREADY_EXISTS the caller owns the open, unowned mutex handle.
DWORD OpenUserMutex(const std::wstring& suffix, HANDLE& mutex);
struct FileRevision { FILE_ID_INFO id{}; FILE_BASIC_INFO basic{}; };
// Compare from offset zero on a fresh handle. Reject a changed file or identity with ERROR_RETRY.
void CheckFileContents(HANDLE file, std::string_view expected, const FileRevision* revision = nullptr);
// Hold the same read/delete handle through comparison and deletion.
void RemoveUnchangedFile(const std::wstring& path, std::string_view expected, const FileRevision* revision = nullptr);
class ResultFile {
    Handle file;
public:
    void Reserve(const std::wstring& path);
    void Write(const Json& result);
    bool Ready() const { return static_cast<bool>(file); }
};
class UserLock {
    Handle mutex;
    bool owned = false;
    DWORD AcquireNamed(const std::wstring& name, DWORD waitMs);
public:
    ~UserLock() { if (owned) ReleaseMutex(mutex.value); }
    DWORD Acquire(const std::wstring& identity, DWORD waitMs = contract::LockWaitMs);
    DWORD AcquireForSetup(const setup::Paths& paths, bool isolated, DWORD waitMs = contract::LockWaitMs);
};
}
