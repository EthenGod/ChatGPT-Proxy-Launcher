// SPDX-License-Identifier: GPL-3.0-only
#include "AutomationIO.h"
#include "NativeMemory.h"
#include "SetupCore.h"
#include <sddl.h>
#include <cwctype>
#include <algorithm>
#include <string_view>
#include <cstring>
#include <utility>

namespace automation {
std::wstring Quote(const std::wstring& value) {
    if (!value.empty() && value.find_first_of(L" \t\r\n\"") == value.npos) return value;
    std::wstring out = L"\""; size_t slashes = 0;
    for (const auto c : value) {
        if (c == L'\\') { ++slashes; continue; }
        out.append(c == L'"' ? slashes * 2 + 1 : slashes, L'\\'); slashes = 0; out += c;
    }
    out.append(slashes * 2, L'\\'); return out + L'"';
}
bool SamePath(const std::wstring& a, const std::wstring& b) { return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_EQUAL; }
std::wstring ParentPath(const std::wstring& path) { const auto n = path.find_last_of(L'\\'); return path.substr(0, n == 2 ? 3 : n); }
std::wstring FullPath(const std::wstring& input) {
    if (input.size() < 3 || input.size() >= 32767 || !((input[0] >= L'A' && input[0] <= L'Z') || (input[0] >= L'a' && input[0] <= L'z')) || input[1] != L':' || input[2] != L'\\'
        || input.find_first_of(L"\"<>|?*\r\n\t/") != input.npos || input.find(L'\0') != input.npos
        || input.find(L':', 2) != input.npos || std::any_of(input.begin(), input.end(), [](wchar_t c) { return c < 32; })) throw Invalid("invalid-local-path");
    // Most paths fit on the stack. Allocate a larger buffer only when Windows requests it.
    wchar_t buffer[MAX_PATH];
    const DWORD needed = GetFullPathNameW(input.c_str(), _countof(buffer), buffer, nullptr);
    if (!needed || needed > 32768) throw Invalid("invalid-local-path");
    std::wstring full;
    if (needed < _countof(buffer)) full.assign(buffer, needed);
    else {
        full.resize(needed);
        const DWORD written = GetFullPathNameW(input.c_str(), needed, full.data(), nullptr);
        if (!written || written >= needed) throw Invalid("invalid-local-path");
        full.resize(written);
    }
    while (full.size() > 3 && full.back() == L'\\') full.pop_back();
    for (size_t start = 3; start < full.size();) {
        const auto end = full.find(L'\\', start);
        const auto part = std::wstring_view(full).substr(start, end == full.npos ? end : end - start);
        auto stem = part.substr(0, part.find(L'.'));
        wchar_t deviceName[4];
        if (stem.size() == 3 || stem.size() == 4) {
            for (size_t i = 0; i < stem.size(); ++i) deviceName[i] = static_cast<wchar_t>(towupper(stem[i]));
            stem = std::wstring_view(deviceName, stem.size());
        }
        if (part.empty() || part.back() == L'.' || part.back() == L' '
            || stem == L"CON" || stem == L"PRN" || stem == L"AUX" || stem == L"NUL"
            || (stem.size() == 4 && (stem.substr(0, 3) == L"COM" || stem.substr(0, 3) == L"LPT")
                && ((stem[3] >= L'1' && stem[3] <= L'9') || stem[3] == L'\u00b9' || stem[3] == L'\u00b2' || stem[3] == L'\u00b3')))
            throw Invalid("invalid-path-component");
        if (end == full.npos) break;
        start = end + 1;
    }
    return full;
}
namespace {
Handle OpenDirectory(const std::wstring& path) {
    return Handle(CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
}
bool FileIdUnsupported(DWORD error) {
    return error == ERROR_INVALID_PARAMETER || error == ERROR_INVALID_FUNCTION || error == ERROR_NOT_SUPPORTED;
}
std::wstring NormalizedHandlePath(HANDLE directory) {
    const DWORD size = GetFinalPathNameByHandleW(directory, nullptr, 0, FILE_NAME_NORMALIZED);
    if (!size || size > 32768) return {};
    std::wstring name(size, L'\0');
    const DWORD written = GetFinalPathNameByHandleW(directory, name.data(), size, FILE_NAME_NORMALIZED);
    if (!written || written >= size) return {};
    name.resize(written);
    return name;
}
bool SameRootDirectory(const std::wstring& root, const std::wstring& candidate) {
    const auto first = OpenDirectory(root), second = OpenDirectory(candidate);
    if (!first || !second) return false;
    FILE_ID_INFO a{}, b{};
    const bool haveFirst = GetFileInformationByHandleEx(first.value, FileIdInfo, &a, sizeof(a)) != FALSE;
    if (!haveFirst && !FileIdUnsupported(GetLastError())) return false;
    const bool haveSecond = GetFileInformationByHandleEx(second.value, FileIdInfo, &b, sizeof(b)) != FALSE;
    if (!haveSecond && !FileIdUnsupported(GetLastError())) return false;
    if (haveFirst && haveSecond)
        return a.VolumeSerialNumber == b.VolumeSerialNumber && std::memcmp(&a.FileId, &b.FileId, sizeof(a.FileId)) == 0;
    BY_HANDLE_FILE_INFORMATION left{}, right{};
    if (GetFileInformationByHandle(first.value, &left) && GetFileInformationByHandle(second.value, &right)
        && left.dwVolumeSerialNumber && right.dwVolumeSerialNumber
        && (left.nFileIndexHigh || left.nFileIndexLow) && (right.nFileIndexHigh || right.nFileIndexLow))
        return left.dwVolumeSerialNumber == right.dwVolumeSerialNumber
            && left.nFileIndexHigh == right.nFileIndexHigh && left.nFileIndexLow == right.nFileIndexLow;
    // Ask the filesystem for the spelling of the opened directories. Compare exactly
    // so a case-sensitive sibling can never be accepted by folding its name.
    const auto name = NormalizedHandlePath(first.value);
    return !name.empty() && name == NormalizedHandlePath(second.value);
}
void CheckPlainComponents(std::wstring full) {
    for (size_t p = 3;;) {
        const auto end = full.find(L'\\', p);
        // Temporarily terminate each prefix instead of allocating a copy for every ancestor.
        if (end != full.npos) full[end] = L'\0';
        const DWORD a = GetFileAttributesW(full.c_str());
        const DWORD error = GetLastError();
        if (end != full.npos) full[end] = L'\\';
        if (a == INVALID_FILE_ATTRIBUTES) {
            if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) throw IoError(error);
        } else if (a & FILE_ATTRIBUTE_REPARSE_POINT) throw Invalid("redirected-path");
        if (end == full.npos) break;
        p = end + 1;
    }
}
}
void CheckPlainPath(const std::wstring& path) { CheckPlainComponents(FullPath(path)); }
void CheckPathUnderRoot(const std::wstring& path, const std::wstring& root) {
    auto full = FullPath(path);
    if (root.empty()) throw Invalid("outside-test-root");
    const auto separator = root.back() == L'\\' ? root.size() - 1 : root.size();
    if (full.size() <= separator + 1 || full[separator] != L'\\') throw Invalid("outside-test-root");
    if (full.compare(0, root.size(), root) != 0) {
        const auto candidate = full.substr(0, root.size());
        if (!SamePath(root, candidate) || !SameRootDirectory(root, candidate)) throw Invalid("outside-test-root");
    }
    CheckPlainComponents(std::move(full));
}
std::string ReadBytes(const std::wstring& path, size_t maximum) {
    CheckPlainPath(path);
    Handle f(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    if (!f) throw IoError(GetLastError());
    BY_HANDLE_FILE_INFORMATION info{};
    if (!GetFileInformationByHandle(f.value, &info)) throw IoError(GetLastError());
    if (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) throw Invalid("not-plain-file");
    if (info.nFileSizeHigh || info.nFileSizeLow > maximum) throw Invalid("file-too-large");
    std::string out(info.nFileSizeLow, '\0'); DWORD got = 0;
    if (!ReadFile(f.value, out.data(), static_cast<DWORD>(out.size()), &got, nullptr) || got != out.size()) throw IoError(ERROR_READ_FAULT);
    return out;
}
void WriteBytes(HANDLE file, const std::string& bytes) {
    DWORD written = 0;
    if (bytes.size() > MAXDWORD) throw Invalid("output-too-large");
    if (!WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr)) throw IoError(GetLastError());
    if (written != bytes.size()) throw IoError(ERROR_WRITE_FAULT);
    if (!FlushFileBuffers(file)) throw IoError(GetLastError());
}
void CheckFileContents(HANDLE file, std::string_view expected, const FileRevision* revision) {
    if (revision) {
        FILE_ID_INFO current{};
        if (!GetFileInformationByHandleEx(file, FileIdInfo, &current, sizeof(current))) throw IoError(GetLastError());
        FILE_BASIC_INFO basic{};
        if (!GetFileInformationByHandleEx(file, FileBasicInfo, &basic, sizeof(basic))) throw IoError(GetLastError());
        if (current.VolumeSerialNumber != revision->id.VolumeSerialNumber
            || memcmp(&current.FileId, &revision->id.FileId, sizeof(current.FileId))
            || basic.ChangeTime.QuadPart != revision->basic.ChangeTime.QuadPart
            || basic.LastWriteTime.QuadPart != revision->basic.LastWriteTime.QuadPart
            || basic.FileAttributes != revision->basic.FileAttributes) throw IoError(ERROR_RETRY);
    }
    BY_HANDLE_FILE_INFORMATION info{};
    if (!GetFileInformationByHandle(file, &info)) throw IoError(GetLastError());
    if ((info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))
        || info.nFileSizeHigh || info.nFileSizeLow != expected.size()) throw IoError(ERROR_RETRY);
    // Compare in bounded stack space instead of allocating a second complete snapshot.
    char buffer[4096];
    for (size_t offset = 0; offset < expected.size();) {
        const DWORD count = static_cast<DWORD>(std::min(sizeof(buffer), expected.size() - offset));
        DWORD got = 0;
        if (!ReadFile(file, buffer, count, &got, nullptr)) throw IoError(GetLastError());
        if (got != count) throw IoError(ERROR_READ_FAULT);
        if (memcmp(buffer, expected.data() + offset, count)) throw IoError(ERROR_RETRY);
        offset += count;
    }
}
void RemoveUnchangedFile(const std::wstring& path, std::string_view expected, const FileRevision* revision) {
    CheckPlainPath(path);
    Handle file(CreateFileW(path.c_str(), GENERIC_READ | DELETE, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    if (!file) throw IoError(GetLastError());
    CheckFileContents(file.value, expected, revision);
    FILE_DISPOSITION_INFO disposition{TRUE};
    if (!SetFileInformationByHandle(file.value, FileDispositionInfo, &disposition, sizeof(disposition)))
        throw IoError(GetLastError());
}
void ResultFile::Reserve(const std::wstring& input) {
    if (file) throw Invalid("result-already-reserved");
    const auto path = FullPath(input); CheckPlainPath(path);
    file.value = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!file) throw IoError(GetLastError());
}
void ResultFile::Write(const Json& result) { WriteBytes(file.value, WriteJson(result)); }
DWORD CurrentUserSid(std::wstring& user) {
    Handle token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token.value)) return GetLastError();
    DWORD size = 0; GetTokenInformation(token.value, TokenUser, nullptr, 0, &size);
    if (!size) return GetLastError();
    std::vector<BYTE> storage(size);
    if (!GetTokenInformation(token.value, TokenUser, storage.data(), size, &size)) return GetLastError();
    LPWSTR sid = nullptr;
    if (!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(storage.data())->User.Sid, &sid)) return GetLastError();
    native::LocalPtr<wchar_t> ownedSid(sid);
    user.assign(sid);
    return ERROR_SUCCESS;
}
DWORD OpenUserMutex(const std::wstring& suffix, HANDLE& mutex) {
    mutex = nullptr;
    std::wstring user;
    if (const DWORD error = CurrentUserSid(user)) return error;
    const auto name = L"Global\\Community.ChatGPTLauncher." + user + L"." + suffix;
    const auto dacl = L"D:P(A;;GA;;;SY)(A;;GA;;;" + user + L")";
    PSECURITY_DESCRIPTOR sd = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(dacl.c_str(), SDDL_REVISION_1, &sd, nullptr)) return GetLastError();
    native::LocalPtr<void> ownedDescriptor(sd);
    SECURITY_ATTRIBUTES security{sizeof(security), sd, FALSE};
    mutex = CreateMutexW(&security, FALSE, name.c_str());
    const DWORD error = GetLastError();
    return mutex && error != ERROR_ALREADY_EXISTS ? ERROR_SUCCESS : error;
}
DWORD UserLock::Acquire(const std::wstring& identity, DWORD waitMs) {
    if (owned) return ERROR_SUCCESS;
    // Preserve the deployed writer lock's per-user/per-root name.
    unsigned long long hash = 14695981039346656037ull;
    for (auto c : identity) { hash ^= static_cast<unsigned>(towupper(c)); hash *= 1099511628211ull; }
    return AcquireNamed(std::to_wstring(hash), waitMs);
}
DWORD UserLock::AcquireForSetup(const setup::Paths& paths, bool isolated, DWORD waitMs) {
    if (!isolated) return Acquire(paths.settingsLink, waitMs);
    if (owned) return ERROR_SUCCESS;
    // The fixture root exists even on the first run, before state/settings.lnk is created.
    const auto rootPath = ParentPath(paths.defaultDirectory);
    const auto root = OpenDirectory(rootPath);
    if (!root) return GetLastError();
    FILE_ID_INFO id{};
    if (!GetFileInformationByHandleEx(root.value, FileIdInfo, &id, sizeof(id))) {
        const DWORD error = GetLastError();
        // Keep the previous locking behavior on filesystems without volume/file IDs.
        if (FileIdUnsupported(error)) return Acquire(paths.settingsLink, waitMs);
        return error;
    }
    auto name = L"Fixture." + std::to_wstring(id.VolumeSerialNumber) + L".";
    constexpr wchar_t hex[] = L"0123456789abcdef";
    for (const BYTE byte : id.FileId.Identifier) {
        name.push_back(hex[byte >> 4]);
        name.push_back(hex[byte & 15]);
    }
    return AcquireNamed(name, waitMs);
}
DWORD UserLock::AcquireNamed(const std::wstring& name, DWORD waitMs) {
    mutex.Reset();
    const DWORD error = OpenUserMutex(name, mutex.value);
    if (!mutex) return error;
    const DWORD result = WaitForSingleObject(mutex.value, waitMs);
    if (result == WAIT_OBJECT_0 || result == WAIT_ABANDONED) { owned = true; return ERROR_SUCCESS; }
    return result == WAIT_TIMEOUT ? ERROR_BUSY : GetLastError();
}
}
