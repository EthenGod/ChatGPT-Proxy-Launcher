// SPDX-License-Identifier: GPL-3.0-only
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "TaskbarShortcut.h"
#include <appmodel.h>
#include <shlobj.h>
#include <propkey.h>
#include <string>
#include <vector>

namespace taskbar {
constexpr wchar_t AppId[] = L"com.openai.codex";
constexpr size_t Capacity = 32768;
template<class T> struct Com {
    T* p = nullptr;
    ~Com() { if (p) p->Release(); }
    T* operator->() const { return p; }
};
struct Apartment {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ~Apartment() { if (SUCCEEDED(hr)) CoUninitialize(); }
};
struct Handle {
    HANDLE h = INVALID_HANDLE_VALUE;
    void Close() { if (h != INVALID_HANDLE_VALUE) { CloseHandle(h); h = INVALID_HANDLE_VALUE; } }
    ~Handle() { Close(); }
};
struct TempFile {
    wchar_t path[MAX_PATH]{};
    ~TempFile() { if (*path) DeleteFileW(path); }
};
bool Same(const std::wstring& a, const std::wstring& b) {
    return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_EQUAL;
}
std::wstring Parent(const std::wstring& path) {
    const size_t pos = path.find_last_of(L'\\');
    return pos == std::wstring::npos ? L"" : path.substr(0, pos == 2 ? 3 : pos);
}
bool LocalPath(const std::wstring& path) {
    return path.size() > 3 && ((path[0] >= L'A' && path[0] <= L'Z')
        || (path[0] >= L'a' && path[0] <= L'z')) && path[1] == L':' && path[2] == L'\\';
}
bool PlainFile(const std::wstring& path) {
    DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT));
}
// Accept only the current package's installation root and the exact package identity.
// No target resolution, disk search or execution of the shortcut is performed.
bool PackageTarget(const std::wstring& target, const std::wstring& application) {
    const std::wstring suffix = L"\\app\\ChatGPT.exe";
    if (!LocalPath(target) || !LocalPath(application) || target.size() <= suffix.size()
        || application.size() <= suffix.size()
        || !Same(target.substr(target.size() - suffix.size()), suffix)
        || !Same(application.substr(application.size() - suffix.size()), suffix)) return false;
    const std::wstring package = target.substr(0, target.size() - suffix.size());
    const std::wstring current = application.substr(0, application.size() - suffix.size());
    if (!Same(Parent(package), Parent(current))) return false;
    const std::wstring name = package.substr(package.find_last_of(L'\\') + 1);
    UINT32 bytes = 0;
    if (PackageIdFromFullName(name.c_str(), PACKAGE_INFORMATION_BASIC, &bytes, nullptr)
        != ERROR_INSUFFICIENT_BUFFER || bytes < sizeof(PACKAGE_ID) || bytes > 131072) return false;
    std::vector<BYTE> storage(bytes);
    if (PackageIdFromFullName(name.c_str(), PACKAGE_INFORMATION_BASIC, &bytes, storage.data())) return false;
    const auto* id = reinterpret_cast<const PACKAGE_ID*>(storage.data());
    return id->name && id->publisherId && id->resourceId && !*id->resourceId
        && wcscmp(id->name, L"OpenAI.Codex") == 0 && wcscmp(id->publisherId, L"2p2nqsd0c76g0") == 0;
}
HRESULT Load(const std::wstring& path, Com<IShellLinkW>& link, Com<IPersistFile>& persist) {
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link.p));
    if (SUCCEEDED(hr)) hr = link->QueryInterface(IID_PPV_ARGS(&persist.p));
    if (SUCCEEDED(hr)) hr = persist->Load(path.c_str(), STGM_READ);
    return hr;
}
bool CorrectId(IShellLinkW* link) {
    Com<IPropertyStore> properties;
    if (FAILED(link->QueryInterface(IID_PPV_ARGS(&properties.p)))) return false;
    PROPVARIANT value{};
    const HRESULT hr = properties->GetValue(PKEY_AppUserModel_ID, &value);
    const bool same = SUCCEEDED(hr) && value.vt == VT_LPWSTR && value.pwszVal
        && wcscmp(value.pwszVal, AppId) == 0;
    PropVariantClear(&value);
    return same;
}
struct Values { std::wstring target, icon, directory, arguments; int iconIndex = 0; };
HRESULT Read(IShellLinkW* link, Values& value) {
    std::vector<wchar_t> text(Capacity);
    HRESULT hr = link->GetPath(text.data(), static_cast<int>(text.size()), nullptr, SLGP_RAWPATH);
    if (hr != S_OK) return FAILED(hr) ? hr : E_FAIL;
    value.target = text.data();
    hr = link->GetIconLocation(text.data(), static_cast<int>(text.size()), &value.iconIndex);
    if (FAILED(hr)) return hr;
    value.icon = text.data();
    hr = link->GetWorkingDirectory(text.data(), static_cast<int>(text.size()));
    if (FAILED(hr)) return hr;
    value.directory = text.data();
    hr = link->GetArguments(text.data(), static_cast<int>(text.size()));
    if (FAILED(hr)) return hr;
    value.arguments = text.data();
    return S_OK;
}
bool Stable(const Values& value, const std::wstring& launcher) {
    return Same(value.target, launcher) && Same(value.icon, launcher) && value.iconIndex == 0
        && Same(value.directory, Parent(launcher));
}
HRESULT LastErrorHr() { return HRESULT_FROM_WIN32(GetLastError()); }

// Paths supplied explicitly so all repair tests use isolated directories.
// S_OK: repaired; S_FALSE: unchanged/skipped; failure: original link left intact.
HRESULT RepairLink(const std::wstring& path, const std::wstring& backup,
    const std::wstring& launcher, const std::wstring& application) {
    if (!LocalPath(path) || !LocalPath(backup) || !LocalPath(launcher)
        || path.size() >= MAX_PATH || backup.size() >= MAX_PATH
        || launcher.size() >= MAX_PATH || Same(launcher, application)
        || !PlainFile(path) || !PlainFile(launcher)) return S_FALSE;
    // Hold a read lock while loading, checking and preparing the replacement.
    Handle original;
    original.h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (original.h == INVALID_HANDLE_VALUE) return LastErrorHr();
    Apartment apartment;
    if (FAILED(apartment.hr)) return apartment.hr;
    Com<IShellLinkW> link;
    Com<IPersistFile> persist;
    HRESULT hr = Load(path, link, persist);
    if (FAILED(hr)) return hr;
    if (!CorrectId(link.p)) return S_FALSE;
    Values before;
    hr = Read(link.p, before);
    if (FAILED(hr)) return hr;
    if (!Same(before.target, launcher) && !PackageTarget(before.target, application)) return S_FALSE;
    if (Stable(before, launcher)) return S_FALSE;
    if (FAILED(hr = link->SetPath(launcher.c_str()))
        || FAILED(hr = link->SetIconLocation(launcher.c_str(), 0))
        || FAILED(hr = link->SetWorkingDirectory(Parent(launcher).c_str()))) return hr;
    // Never persist this invocation's arguments. Retain the shortcut's own arguments.
    TempFile temp;
    if (!GetTempFileNameW(Parent(path).c_str(), L"cgt", 0, temp.path)) return LastErrorHr();
    if (FAILED(hr = persist->Save(temp.path, FALSE))) return hr;
    Com<IShellLinkW> verify;
    Com<IPersistFile> verifyPersist;
    if (FAILED(hr = Load(temp.path, verify, verifyPersist))) return hr;
    Values after;
    if (FAILED(hr = Read(verify.p, after))) return hr;
    if (!CorrectId(verify.p) || !Stable(after, launcher) || after.arguments != before.arguments)
        return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    // Keep the first original outside the Start menu. Never overwrite this backup.
    const DWORD attributes = GetFileAttributesW(backup.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        const DWORD error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) return HRESULT_FROM_WIN32(error);
        const int created = SHCreateDirectoryExW(nullptr, Parent(backup).c_str(), nullptr);
        if (created != ERROR_SUCCESS && created != ERROR_ALREADY_EXISTS && created != ERROR_FILE_EXISTS)
            return HRESULT_FROM_WIN32(created);
        TempFile backupTemp;
        if (!GetTempFileNameW(Parent(backup).c_str(), L"cgb", 0, backupTemp.path)) return LastErrorHr();
        if (!CopyFileW(path.c_str(), backupTemp.path, FALSE)) return LastErrorHr();
        if (!MoveFileExW(backupTemp.path, backup.c_str(), MOVEFILE_WRITE_THROUGH)) return LastErrorHr();
        *backupTemp.path = L'\0';
    } else {
        if (!PlainFile(backup)) return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
        Com<IShellLinkW> saved; Com<IPersistFile> savedPersist;
        Values savedValues;
        if (FAILED(hr = Load(backup, saved, savedPersist))) return hr;
        if (!CorrectId(saved.p)) return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
        if (FAILED(hr = Read(saved.p, savedValues))) return hr;
        if (!Same(savedValues.target, launcher) && !PackageTarget(savedValues.target, application))
            return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    }
    // Single-file atomic replacement; a failed move never requires deleting the original.
    original.Close();
    if (!MoveFileExW(temp.path, path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        return LastErrorHr();
    *temp.path = L'\0';
    SHChangeNotify(SHCNE_UPDATEITEM, SHCNF_PATHW | SHCNF_FLUSHNOWAIT, path.c_str(), nullptr);
    return S_OK;
}
std::wstring Folder(REFKNOWNFOLDERID id) {
    PWSTR value = nullptr;
    if (FAILED(SHGetKnownFolderPath(id, KF_FLAG_DONT_VERIFY, nullptr, &value))) return L"";
    // Free the COM allocation even if constructing the string throws.
    struct Free { PWSTR p; ~Free() { CoTaskMemFree(p); } } free{ value };
    return value;
}
} // namespace taskbar

void RepairTaskbarShortcut(PCWSTR application) noexcept {
    try {
        wchar_t module[MAX_PATH]{};
        const DWORD length = GetModuleFileNameW(nullptr, module, MAX_PATH);
        if (!length || length >= MAX_PATH) return;
        const std::wstring programs = taskbar::Folder(FOLDERID_Programs);
        // Reject redirected network paths before even probing file attributes.
        if (!taskbar::LocalPath(programs)) return;
        const std::wstring link = programs + L"\\ChatGPT.lnk";
        if (link.size() >= MAX_PATH) return;
        if (!taskbar::PlainFile(link)) return;
        const std::wstring local = taskbar::Folder(FOLDERID_LocalAppData);
        if (local.empty()) return;
        taskbar::RepairLink(link, local + L"\\ChatGPTProxyLauncher\\TaskbarShortcut.original.lnk",
            module, application);
    } catch (...) {
        // Icon maintenance must not prevent normal launching, even on allocation failure.
    }
}
