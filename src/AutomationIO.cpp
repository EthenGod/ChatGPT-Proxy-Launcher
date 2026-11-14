// SPDX-License-Identifier: GPL-3.0-only
#include "AutomationIO.h"
#include <sddl.h>
#include <cwctype>
#include <algorithm>

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
    std::wstring full(32768, L'\0');
    const DWORD n = GetFullPathNameW(input.c_str(), static_cast<DWORD>(full.size()), full.data(), nullptr);
    if (!n || n >= full.size()) throw Invalid("invalid-local-path");
    full.resize(n);
    while (full.size() > 3 && full.back() == L'\\') full.pop_back();
    for (size_t start = 3; start < full.size();) {
        const auto end = full.find(L'\\', start);
        const auto part = full.substr(start, end == full.npos ? end : end - start);
        auto stem = part.substr(0, part.find(L'.'));
        for (auto& c : stem) c = static_cast<wchar_t>(towupper(c));
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
void CheckPlainPath(const std::wstring& path) {
    const auto full = FullPath(path);
    for (size_t p = 3;;) {
        const auto end = full.find(L'\\', p);
        const auto part = full.substr(0, end);
        const DWORD a = GetFileAttributesW(part.c_str());
        if (a == INVALID_FILE_ATTRIBUTES) {
            const DWORD e = GetLastError();
            if (e != ERROR_FILE_NOT_FOUND && e != ERROR_PATH_NOT_FOUND) throw IoError(e);
        } else if (a & FILE_ATTRIBUTE_REPARSE_POINT) throw Invalid("redirected-path");
        if (end == full.npos) break;
        p = end + 1;
    }
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
void ResultFile::Reserve(const std::wstring& input) {
    if (file) throw Invalid("result-already-reserved");
    const auto path = FullPath(input); CheckPlainPath(path);
    file.value = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!file) throw IoError(GetLastError());
}
void ResultFile::Write(const Json& result) { WriteBytes(file.value, WriteJson(result)); }
DWORD UserLock::Acquire(const std::wstring& identity, DWORD waitMs) {
    if (owned) return ERROR_SUCCESS;
    mutex.Reset();
    Handle token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token.value)) return GetLastError();
    DWORD size = 0; GetTokenInformation(token.value, TokenUser, nullptr, 0, &size);
    if (!size) return GetLastError();
    std::vector<BYTE> storage(size);
    if (!GetTokenInformation(token.value, TokenUser, storage.data(), size, &size)) return GetLastError();
    LPWSTR sid = nullptr;
    if (!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(storage.data())->User.Sid, &sid)) return GetLastError();
    std::wstring user(sid); LocalFree(sid);
    // Stable per-user/per-root identity across sessions, independent of GUI lifetime.
    unsigned long long hash = 14695981039346656037ull;
    for (auto c : identity) { hash ^= static_cast<unsigned>(towupper(c)); hash *= 1099511628211ull; }
    const auto name = L"Global\\Community.ChatGPTLauncher." + user + L"." + std::to_wstring(hash);
    const auto dacl = L"D:P(A;;GA;;;SY)(A;;GA;;;" + user + L")";
    PSECURITY_DESCRIPTOR sd = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(dacl.c_str(), SDDL_REVISION_1, &sd, nullptr)) return GetLastError();
    SECURITY_ATTRIBUTES security{sizeof(security), sd, FALSE};
    mutex.value = CreateMutexW(&security, FALSE, name.c_str());
    const DWORD creationError = GetLastError(); LocalFree(sd);
    if (!mutex) return creationError;
    const DWORD result = WaitForSingleObject(mutex.value, waitMs);
    if (result == WAIT_OBJECT_0 || result == WAIT_ABANDONED) { owned = true; return ERROR_SUCCESS; }
    return result == WAIT_TIMEOUT ? ERROR_BUSY : GetLastError();
}
}
