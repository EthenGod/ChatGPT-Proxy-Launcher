// SPDX-License-Identifier: GPL-3.0-only
#include "SetupCore.h"
#include "Localization.h"
#include "CodexEnv.h"
#include "StartupApps.h"
#include "DeploymentDiscovery.h"
#include "AppVersion.h"
#include "NativeMemory.h"
#include <shlobj.h>
#include <shlwapi.h>
#include <aclapi.h>
#include <sddl.h>
#include <wrl/client.h>
#include <algorithm>
#include <utility>
#include <new>

namespace setup {
namespace {
using Microsoft::WRL::ComPtr;
struct Handle {
    HANDLE h = INVALID_HANDLE_VALUE;
    Handle() = default;
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    ~Handle() { if (h != INVALID_HANDLE_VALUE) CloseHandle(h); }
};
Result Fail(DWORD code, const std::wstring& what) {
    return { code ? code : ERROR_GEN_FAILURE, what + L"\n" + ErrorText(code) };
}
Result Hr(HRESULT hr, const std::wstring& what) {
    return Fail(static_cast<DWORD>(hr), what);
}
Result Conflict(const std::wstring& message, const std::wstring& directory) {
    Result result = Fail(ERROR_ALREADY_EXISTS, message);
    result.conflictDirectory = directory;
    return result;
}
bool Same(const std::wstring& a, const std::wstring& b) {
    return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_EQUAL;
}
bool IsMissing(DWORD code) { return code == ERROR_FILE_NOT_FOUND || code == ERROR_PATH_NOT_FOUND; }
bool Exists(const std::wstring& path) { return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES; }
bool Owned(const Link& link) { return link.description == Marker; }
std::wstring Parent(const std::wstring& path) {
    const size_t n = path.find_last_of(L"\\/");
    return n == std::wstring::npos ? L"" : path.substr(0, n == 2 ? 3 : n);
}
Result EnsureDirectory(const std::wstring& path) {
    const int error = SHCreateDirectoryExW(nullptr, path.c_str(), nullptr);
    if (error == ERROR_SUCCESS || error == ERROR_ALREADY_EXISTS || error == ERROR_FILE_EXISTS) {
        const DWORD attributes = GetFileAttributesW(path.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY)) return {};
    }
    return Fail(static_cast<DWORD>(error), l10n::Format("error.files.createdirectory", {{"path", path}}));
}
struct Snapshot { bool exists = false; std::vector<BYTE> data; };

Result OpenSnapshot(const std::wstring& path, Handle& handle, DWORD& length, bool requirePlainFile = false) {
    handle.h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        requirePlainFile ? FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS : FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle.h == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        return IsMissing(error) ? Result{} : Fail(error, l10n::Format("error.files.read", {{"path", path}}));
    }
    if (requirePlainFile) {
        FILE_ATTRIBUTE_TAG_INFO info{};
        if (!GetFileInformationByHandleEx(handle.h, FileAttributeTagInfo, &info, sizeof(info)))
            return Fail(GetLastError(), l10n::Text("error.env.filetype"));
        if (info.FileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))
            return Fail(ERROR_BAD_PATHNAME, l10n::Text("error.env.filetype"));
    }
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(handle.h, &size)) return Fail(GetLastError(), l10n::Text("error.files.size"));
    if (size.QuadPart < 0 || size.QuadPart > 32 * 1024 * 1024)
        return Fail(ERROR_FILE_TOO_LARGE, l10n::Format("error.files.toolarge", {{"path", path}}));
    length = static_cast<DWORD>(size.QuadPart);
    return {};
}
Result ReadExact(HANDLE handle, const std::wstring& path, BYTE* buffer, DWORD length) {
    DWORD count = 0;
    if (!ReadFile(handle, buffer, length, &count, nullptr)) {
        const DWORD error = GetLastError();
        return Fail(error, l10n::Format("error.files.contents", {{"path", path}}));
    }
    if (count != length) return Fail(ERROR_READ_FAULT, l10n::Format("error.files.incomplete", {{"path", path}}));
    return {};
}
Result ReadFileBytes(const std::wstring& path, Snapshot& file, bool requirePlainFile = false) {
    file = {};
    Handle handle;
    DWORD length = 0;
    Result result = OpenSnapshot(path, handle, length, requirePlainFile);
    if (!result || handle.h == INVALID_HANDLE_VALUE) return result;
    file.data.resize(length);
    result = ReadExact(handle.h, path, file.data.data(), length);
    if (result) file.exists = true;
    return result;
}

enum class FileMatch { Missing, Different, Identical };
Result CompareLauncher(const std::wstring& path, Payload payload, FileMatch& match, Snapshot* backup = nullptr) {
    match = FileMatch::Missing;
    Handle handle;
    DWORD length = 0;
    Result result = OpenSnapshot(path, handle, length);
    if (!result || handle.h == INVALID_HANDLE_VALUE) return result;
    match = FileMatch::Different;
    if (length == payload.size) {
        BYTE block[8 * 1024];
        DWORD offset = 0;
        while (offset < length) {
            const DWORD count = (std::min)(length - offset, static_cast<DWORD>(sizeof(block)));
            result = ReadExact(handle.h, path, block, count);
            if (!result) return result;
            if (memcmp(block, payload.data + offset, count) != 0) break;
            offset += count;
        }
        if (offset == length) {
            match = FileMatch::Identical;
            return {};
        }
        if (backup && !SetFilePointerEx(handle.h, {}, nullptr, FILE_BEGIN)) {
            const DWORD error = GetLastError();
            return Fail(error, l10n::Format("error.files.read", {{"path", path}}));
        }
    }
    // Use the same read handle so the rollback copy belongs to the file just compared.
    if (backup) {
        backup->data.resize(length);
        result = ReadExact(handle.h, path, backup->data.data(), length);
        if (result) backup->exists = true;
    }
    return result;
}
// Each temporary object owns exactly one newly-created, explicit file path.
struct TempFile {
    std::wstring path;
    ~TempFile() { if (!path.empty()) DeleteFileW(path.c_str()); }
};
Result NewTemp(const std::wstring& target, TempFile& temp) {
    temp.path.reserve(MAX_PATH); // No allocation may fail after the file is created.
    wchar_t name[MAX_PATH]{};
    if (!GetTempFileNameW(Parent(target).c_str(), L"cgl", 0, name))
        return Fail(GetLastError(), l10n::Text("error.files.createtemp"));
    temp.path = name;
    return {};
}
Result CommitTemp(TempFile& temp, const std::wstring& target, bool replace = true) {
    if (!MoveFileExW(temp.path.c_str(), target.c_str(), (replace ? MOVEFILE_REPLACE_EXISTING : 0) | MOVEFILE_WRITE_THROUGH)) {
        const DWORD error = GetLastError();
        if (!replace && (error == ERROR_ALREADY_EXISTS || error == ERROR_FILE_EXISTS))
            return Conflict(l10n::Format("error.files.lateconflict", {{"path", target}}), Parent(target));
        return Fail(error, l10n::Format("error.files.save", {{"path", target}}));
    }
    temp.path.clear();
    return {};
}
Result WriteAtomicBytes(const std::wstring& path, const BYTE* data, size_t size, bool replace) {
    TempFile temp;
    Result result = NewTemp(path, temp);
    if (!result) return result;
    {
        Handle handle;
        handle.h = CreateFileW(temp.path.c_str(), GENERIC_WRITE, 0, nullptr, TRUNCATE_EXISTING,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle.h == INVALID_HANDLE_VALUE) return Fail(GetLastError(), l10n::Text("error.files.opentemp"));
        DWORD written = 0;
        if (!WriteFile(handle.h, data, static_cast<DWORD>(size), &written, nullptr)
            || written != size || !FlushFileBuffers(handle.h))
            return Fail(GetLastError(), l10n::Text("error.files.write"));
    }
    return CommitTemp(temp, path, replace);
}
Result WriteAtomic(const std::wstring& path, const std::vector<BYTE>& data, bool replace = true) {
    return WriteAtomicBytes(path, data.data(), data.size(), replace);
}
std::string_view Bytes(const std::vector<BYTE>& data) {
    return {reinterpret_cast<const char*>(data.data()), data.size()};
}
constexpr SECURITY_INFORMATION EnvAccess = OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION
    | LABEL_SECURITY_INFORMATION | ATTRIBUTE_SECURITY_INFORMATION | SCOPE_SECURITY_INFORMATION;
DWORD DescribeEnvAccess(PSECURITY_DESCRIPTOR sd, bool existing, native::LocalPtr<wchar_t>& text) {
    // Ignore the automatic-inheritance bookkeeping bits. Keep protection flags, principals and ACE order.
    constexpr SECURITY_DESCRIPTOR_CONTROL bookkeeping = SE_DACL_AUTO_INHERITED | SE_SACL_AUTO_INHERITED
        | SE_DACL_AUTO_INHERIT_REQ | SE_SACL_AUTO_INHERIT_REQ;
    if (!SetSecurityDescriptorControl(sd, bookkeeping, 0)) return GetLastError();
    PWSTR raw = nullptr;
    if (!ConvertSecurityDescriptorToStringSecurityDescriptorW(sd, SDDL_REVISION_1, OWNER_SECURITY_INFORMATION
        | DACL_SECURITY_INFORMATION | (existing ? GROUP_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION : 0), &raw, nullptr))
        return GetLastError();
    text.reset(raw);
    return ERROR_SUCCESS;
}
DWORD CheckStageAccess(HANDLE stage, PSECURITY_DESCRIPTOR expected, bool existing) {
    PSECURITY_DESCRIPTOR raw = nullptr;
    DWORD error = GetSecurityInfo(stage, SE_FILE_OBJECT, EnvAccess, nullptr, nullptr, nullptr, nullptr, &raw);
    if (error) return error;
    native::LocalPtr<void> actual(raw);
    native::LocalPtr<wchar_t> before, after;
    if ((error = DescribeEnvAccess(expected, existing, before)) || (error = DescribeEnvAccess(raw, existing, after)))
        return error;
    return wcscmp(before.get(), after.get()) ? ERROR_INVALID_SECURITY_DESCR : ERROR_SUCCESS;
}
// Keep the replaced file itself until the transaction ends. This also preserves its streams and security on undo.
struct EnvChange {
    std::wstring backupPath;
    std::wstring stagedPath;
    std::vector<BYTE> restoreName, retainName;
    automation::Handle original; // Deny writes and renames to the backup until commit or undo.
    automation::Handle pin; // Attribute-only handle keeps the published file ID from being recycled.
    automation::FileRevision revision;
    bool installed = false;

    Result DiscardOriginal() {
        if (!original) return {};
        FILE_DISPOSITION_INFO dispose{TRUE};
        if (!SetFileInformationByHandle(original.value, FileDispositionInfo, &dispose, sizeof(dispose)))
            return Fail(GetLastError(), l10n::Text("error.files.write"));
        original.Reset();
        return {};
    }

    Result Undo(const std::wstring& path, const std::vector<BYTE>& expected) {
        bool stagedCopy = !installed && !backupPath.empty();
        auto retained = [&](DWORD code) {
            auto result = Fail(code, l10n::Format(original ? "error.env.restore" : "error.env.backupunverified",
                {{"path", backupPath}}));
            if (stagedCopy) result.message += L"\n" + l10n::Format("error.env.retainedstage", {{"path", stagedPath}});
            return result;
        };
        if (!backupPath.empty() && !original) return retained(ERROR_RETRY);
        if (!original) {
            try { automation::RemoveUnchangedFile(path, Bytes(expected), &revision); }
            catch (const automation::IoError& e) { return Fail(e.code, l10n::Text("error.env.concurrent")); }
            pin.Reset();
            return {};
        }
        automation::Handle published;
        auto rename = [](HANDLE file, std::vector<BYTE>& name) {
            return SetFileInformationByHandle(file, FileRenameInfo, name.data(), static_cast<DWORD>(name.size()));
        };
        if (installed) {
            published.Reset(CreateFileW(path.c_str(), GENERIC_READ | DELETE, FILE_SHARE_READ,
                nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
            if (!published) return retained(GetLastError());
            try { automation::CheckFileContents(published.value, Bytes(expected), &revision); }
            catch (const automation::IoError& e) { return retained(e.code); }
            // Keep the verified version and its handle until the original has recovered its name.
            if (!rename(published.value, retainName)) return retained(GetLastError());
            stagedCopy = true;
        }
        // Neither rename may replace a file that arrived while the name was vacant.
        if (!rename(original.value, restoreName)) {
            const DWORD error = GetLastError();
            if (published && rename(published.value, restoreName)) stagedCopy = false;
            return retained(error);
        }
        if (published) {
            FILE_DISPOSITION_INFO dispose{TRUE};
            if (!SetFileInformationByHandle(published.value, FileDispositionInfo, &dispose, sizeof(dispose)))
                return Fail(GetLastError(), l10n::Format("error.env.retainedstage", {{"path", stagedPath}}));
        } else {
            try { automation::RemoveUnchangedFile(stagedPath, Bytes(expected), &revision); }
            catch (const automation::IoError& e) {
                return Fail(e.code, l10n::Format("error.env.retainedstage", {{"path", stagedPath}}));
            }
        }
        return {};
    }
};
void PrepareEnvRename(std::vector<BYTE>& buffer, const std::wstring& path) {
    buffer.resize(sizeof(FILE_RENAME_INFO) + path.size() * sizeof(wchar_t));
    auto& rename = *reinterpret_cast<FILE_RENAME_INFO*>(buffer.data());
    rename.FileNameLength = static_cast<DWORD>(path.size() * sizeof(wchar_t));
    memcpy(rename.FileName, path.data(), rename.FileNameLength);
}
Result WriteEnv(const std::wstring& path, const Snapshot& before, const std::vector<BYTE>& bytes, EnvChange& change) {
    automation::Handle current;
    PSECURITY_DESCRIPTOR sd = nullptr;
    FILE_ID_INFO previousId{};
    DWORD attributes = FILE_ATTRIBUTE_NORMAL;
    if (before.exists) {
        current.Reset(CreateFileW(path.c_str(), GENERIC_READ | READ_CONTROL, FILE_SHARE_READ | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
        if (!current) return Fail(GetLastError(), l10n::Text("error.env.concurrent"));
        try { automation::CheckFileContents(current.value, Bytes(before.data)); }
        catch (const automation::IoError& e) { return Fail(e.code, l10n::Text("error.env.concurrent")); }
        BY_HANDLE_FILE_INFORMATION info{};
        if (!GetFileInformationByHandleEx(current.value, FileIdInfo, &previousId, sizeof(previousId)))
            return Fail(GetLastError(), l10n::Text("error.env.protection"));
        if (!GetFileInformationByHandle(current.value, &info)) return Fail(GetLastError(), l10n::Text("error.env.protection"));
        // A separately encrypted staging file need not have the same EFS recipients.
        if (info.dwFileAttributes & FILE_ATTRIBUTE_ENCRYPTED) return Fail(ERROR_NOT_SUPPORTED, l10n::Text("error.env.protection"));
        if (info.dwFileAttributes & FILE_ATTRIBUTE_READONLY) return Fail(ERROR_ACCESS_DENIED, l10n::Text("error.env.protection"));
        attributes = info.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED);
        const DWORD error = GetSecurityInfo(current.value, SE_FILE_OBJECT, EnvAccess, nullptr, nullptr, nullptr, nullptr, &sd);
        if (error) return Fail(error, l10n::Text("error.env.protection"));
    } else {
        std::wstring user;
        if (const DWORD error = automation::CurrentUserSid(user)) return Fail(error, l10n::Text("error.env.protection"));
        const auto sddl = L"O:" + user + L"D:P(A;;FA;;;" + user + L")";
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &sd, nullptr))
            return Fail(GetLastError(), l10n::Text("error.env.protection"));
    }
    native::LocalPtr<void> descriptor(sd);
    GUID guid{};
    wchar_t name[40]{};
    if (FAILED(CoCreateGuid(&guid)) || !StringFromGUID2(guid, name, _countof(name)))
        return Fail(ERROR_GEN_FAILURE, l10n::Text("error.files.createtemp"));
    auto stagedPath = Join(Parent(path), std::wstring(name) + L".tmp");
    auto backupPath = stagedPath + L".bak";
    if (before.exists) {
        change.stagedPath = stagedPath;
        PrepareEnvRename(change.restoreName, path);
        PrepareEnvRename(change.retainName, stagedPath);
    }
    TempFile stage;
    SECURITY_ATTRIBUTES security{sizeof(security), sd, FALSE};
    {
        automation::Handle writer(CreateFileW(stagedPath.c_str(), GENERIC_WRITE | FILE_READ_ATTRIBUTES | READ_CONTROL, 0,
            &security, CREATE_NEW, attributes, nullptr));
        if (!writer) return Fail(GetLastError(), l10n::Text("error.files.createtemp"));
        stage.path.swap(stagedPath);
        DWORD flags = 0;
        if (!GetVolumeInformationByHandleW(writer.value, nullptr, 0, nullptr, nullptr, &flags, nullptr, 0))
            return Fail(GetLastError(), l10n::Text("error.env.protection"));
        if (!(flags & FILE_PERSISTENT_ACLS)) return Fail(ERROR_NOT_SUPPORTED, l10n::Text("error.env.protection"));
        if (const DWORD error = CheckStageAccess(writer.value, sd, before.exists))
            return Fail(error, l10n::Text("error.env.protection"));
        if (!GetFileInformationByHandleEx(writer.value, FileIdInfo, &change.revision.id, sizeof(change.revision.id)))
            return Fail(GetLastError(), l10n::Text("error.env.protection"));
        constexpr FILE_ID_128 emptyId{};
        if (!memcmp(&change.revision.id.FileId, &emptyId, sizeof(emptyId))) return Fail(ERROR_NOT_SUPPORTED, l10n::Text("error.env.protection"));
        change.pin.Reset(ReOpenFile(writer.value, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0));
        if (!change.pin) return Fail(GetLastError(), l10n::Text("error.env.protection"));
        DWORD written = 0;
        if (!WriteFile(writer.value, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr)
            || written != bytes.size() || !FlushFileBuffers(writer.value))
            return Fail(GetLastError(), l10n::Text("error.files.write"));
    }
    if (before.exists) {
        // Strict merge preserves the original DACL, compression and named streams. Never ignore merge errors.
        const BOOL replaced = ReplaceFileW(path.c_str(), stage.path.c_str(), backupPath.c_str(), 0, nullptr, nullptr);
        const DWORD error = replaced ? ERROR_SUCCESS : GetLastError();
        if (!replaced && error != ERROR_UNABLE_TO_MOVE_REPLACEMENT_2)
            return Fail(error, l10n::Format("error.files.save", {{"path", path}}));
        change.installed = replaced != FALSE;
        stage.path.clear();
        change.backupPath.swap(backupPath);
        automation::Handle backup(CreateFileW(change.backupPath.c_str(), DELETE | FILE_READ_ATTRIBUTES, FILE_SHARE_READ,
            nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
        if (!backup) return Fail(GetLastError(), l10n::Text("error.env.concurrent"));
        FILE_ID_INFO displaced{};
        if (!GetFileInformationByHandleEx(backup.value, FileIdInfo, &displaced, sizeof(displaced)))
            return Fail(GetLastError(), l10n::Text("error.env.concurrent"));
        if (displaced.VolumeSerialNumber != previousId.VolumeSerialNumber
            || memcmp(&displaced.FileId, &previousId.FileId, sizeof(displaced.FileId)))
            return Fail(ERROR_RETRY, l10n::Text("error.env.concurrent"));
        change.original.Reset(std::exchange(backup.value, INVALID_HANDLE_VALUE));
    } else {
        Result result = CommitTemp(stage, path, false);
        if (!result) return result;
        change.installed = true;
    }
    if (!GetFileInformationByHandleEx(change.pin.value, FileBasicInfo, &change.revision.basic, sizeof(change.revision.basic)))
        return Fail(GetLastError(), l10n::Text("error.env.verify"));
    if (!change.installed)
        return Fail(ERROR_UNABLE_TO_MOVE_REPLACEMENT_2, l10n::Format("error.files.save", {{"path", path}}));
    return {};
}
Result Restore(const std::wstring& path, const Snapshot& snapshot, bool replace = true) {
    if (snapshot.exists) return WriteAtomic(path, snapshot.data, replace);
    if (DeleteFileW(path.c_str()) || IsMissing(GetLastError())) return {};
    return Fail(GetLastError(), l10n::Format("error.files.undo", {{"path", path}}));
}
Result WriteLink(const std::wstring& path, const Link& value, bool replace = true) {
    ComPtr<IShellLinkW> link;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link));
    if (FAILED(hr)) return Hr(hr, l10n::Text("error.link.create"));
    if (value.target.find(L'%') == std::wstring::npos) hr = link->SetPath(value.target.c_str());
    else {
        // SetPath can expand %NAME% inside a literal filename. Bind the actual filesystem item instead.
        PIDLIST_ABSOLUTE item = nullptr;
        hr = SHParseDisplayName(value.target.c_str(), nullptr, &item, 0, nullptr);
        if (SUCCEEDED(hr)) hr = link->SetIDList(item);
        CoTaskMemFree(item);
    }
    if (FAILED(hr) || FAILED(hr = link->SetArguments(value.arguments.c_str()))
        || FAILED(hr = link->SetWorkingDirectory(value.directory.c_str()))
        || FAILED(hr = link->SetDescription(value.description.c_str()))
        || FAILED(hr = link->SetIconLocation(value.target.c_str(), 0))
        || FAILED(hr = link->SetShowCmd(SW_SHOWNORMAL))) return Hr(hr, l10n::Text("error.link.properties"));
    ComPtr<IPersistFile> persist;
    if (FAILED(hr = link->QueryInterface(IID_PPV_ARGS(&persist)))) return Hr(hr, l10n::Text("error.link.save"));
    TempFile temp;
    Result result = NewTemp(path, temp);
    if (!result) return result;
    if (FAILED(hr = persist->Save(temp.path.c_str(), TRUE))) return Hr(hr, l10n::Text("error.link.write"));
    Link read;
    result = ReadLink(temp.path, read);
    if (!result) return result;
    if (!Same(read.target, value.target) || read.arguments != value.arguments
        || !Same(read.directory, value.directory) || read.description != value.description)
        return Fail(ERROR_INVALID_DATA, l10n::Text("error.link.verify"));
    return CommitTemp(temp, path, replace);
}
} // namespace

std::wstring ErrorText(DWORD error) {
    PWSTR message = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    if (!FormatMessageW(flags, nullptr, error, l10n::SystemLanguage(), reinterpret_cast<PWSTR>(&message), 0, nullptr))
        FormatMessageW(flags, nullptr, error, 0, reinterpret_cast<PWSTR>(&message), 0, nullptr);
    native::LocalPtr<wchar_t> ownedMessage(message);
    std::wstring result = message ? message : l10n::Text("error.system.nodetails");
    while (!result.empty() && (result.back() == L'\r' || result.back() == L'\n')) result.pop_back();
    return l10n::Format("error.system.details", {{"message", result}, {"code", std::to_wstring(error)}});
}
std::wstring Join(const std::wstring& directory, const std::wstring& file) {
    return directory + (!directory.empty() && directory.back() == L'\\' ? L"" : L"\\") + file;
}
Result GetPaths(Paths& paths) {
    PWSTR local = nullptr, startup = nullptr;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local);
    native::TaskPtr<wchar_t> ownedLocal(local);
    if (FAILED(hr)) return Hr(hr, l10n::Text("error.paths.localappdata"));
    paths.defaultDirectory = Join(local, companions::Root);
    paths.settingsLink = Join(paths.defaultDirectory, automation::contract::SettingsLink);
    hr = SHGetKnownFolderPath(FOLDERID_Startup, 0, nullptr, &startup);
    native::TaskPtr<wchar_t> ownedStartup(startup);
    if (FAILED(hr)) return Hr(hr, l10n::Text("error.paths.startup"));
    paths.startupLink = Join(startup, LinkName);
    paths.legacyStartupLink = Join(startup, LegacyLinkName);
    PWSTR desktop = nullptr;
    hr = SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &desktop);
    native::TaskPtr<wchar_t> ownedDesktop(desktop);
    // The optional desktop entry must not prevent deployment when the desktop is unavailable.
    paths.desktopLink = SUCCEEDED(hr) ? Join(desktop, LinkName) : L"";
    paths.legacyDesktopLink = SUCCEEDED(hr) ? Join(desktop, LegacyLinkName) : L"";
    PWSTR profile = nullptr;
    hr = SHGetKnownFolderPath(FOLDERID_Profile, 0, nullptr, &profile);
    native::TaskPtr<wchar_t> ownedProfile(profile);
    paths.codexEnvFile = SUCCEEDED(hr) ? Join(Join(profile, L".codex"), L".env") : L"";
    return {};
}
Result EnsureCompanionFolder(const Paths& paths, std::wstring& folder) {
    if (paths.defaultDirectory.empty()) return Fail(ERROR_PATH_NOT_FOUND, l10n::Text("error.paths.companions"));
    folder = Join(paths.defaultDirectory, companions::Folder);
    Result result = EnsureDirectory(folder);
    if (!result) return result;
    if (GetFileAttributesW(folder.c_str()) & FILE_ATTRIBUTE_REPARSE_POINT)
        return Fail(ERROR_BAD_PATHNAME, l10n::Text("error.paths.redirectedstartup"));
    return {};
}
std::wstring CompanionShortcutPath(const std::wstring& folder, const std::wstring& program) {
    const std::wstring name = program.substr(program.find_last_of(L'\\') + 1);
    return Join(folder, name.substr(0, name.find_last_of(L'.')) + L".lnk");
}
Result AddCompanionProgram(const Paths& paths, const std::wstring& program) {
    auto targetKind = companions::FileKind(program.c_str());
    const bool importing = targetKind == companions::Kind::Link;
    if (!companions::LocalPath(program.c_str()) || (!importing && !companions::ValidTarget(program.c_str(), L"", L"")))
        return Fail(ERROR_BAD_PATHNAME, l10n::Text("error.program.unsupported"));
    DWORD attrs = GetFileAttributesW(program.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY))
        return Fail(ERROR_FILE_NOT_FOUND, l10n::Text("error.program.missing"));
    if (importing) {
        Link source;
        Result read = ReadLink(program, source, true);
        if (!read) return read;
        if (companions::FileKind(source.target.c_str()) == companions::Kind::Batch && !source.arguments.empty())
            return Fail(ERROR_INVALID_PARAMETER, l10n::Text("error.program.batchargs"));
        if (!companions::ValidTarget(source.target.c_str(), source.arguments.c_str(), source.directory.c_str()))
            return Fail(ERROR_BAD_PATHNAME, l10n::Text("error.program.linktarget"));
        targetKind = companions::FileKind(source.target.c_str());
        DWORD targetAttrs = GetFileAttributesW(source.target.c_str());
        if (targetAttrs == INVALID_FILE_ATTRIBUTES || (targetAttrs & FILE_ATTRIBUTE_DIRECTORY))
            return Fail(ERROR_FILE_NOT_FOUND, l10n::Text("error.program.linkmissing"));
    }
    if (targetKind != companions::Kind::Exe && companions::FindHost(targetKind).empty()) {
        if (PCWSTR host = companions::PathHost(targetKind))
            return Fail(ERROR_FILE_NOT_FOUND, l10n::Format("error.program.runtime", {{"runtime", host}}));
        return Fail(ERROR_FILE_NOT_FOUND, l10n::Text("error.program.scripthost"));
    }
    std::wstring folder;
    Result result = EnsureCompanionFolder(paths, folder);
    if (!result) return result;
    if (Same(Parent(program), folder)) return {}; // Already in the dispatch list.
    const std::wstring destination = CompanionShortcutPath(folder, program);
    if (Exists(destination)) {
        if (importing) {
            Snapshot a, b;
            if (ReadFileBytes(program, a) && ReadFileBytes(destination, b) && a.exists && b.exists && a.data == b.data) return {};
        } else {
            Link existing;
            if (ReadLink(destination, existing) && Same(existing.target, program)) return {};
        }
        return Conflict(l10n::Text("error.program.conflict"), folder);
    }
    if (importing) {
        TempFile temp;
        result = NewTemp(destination, temp);
        if (!result) return result;
        if (!CopyFileW(program.c_str(), temp.path.c_str(), FALSE)) return Fail(GetLastError(), l10n::Text("error.program.copy"));
        return CommitTemp(temp, destination, false);
    }
    return WriteLink(destination, {program, L"", Parent(program), l10n::Text("program.description")}, false);
}
Result NormalizeDirectory(const std::wstring& input, std::wstring& output) {
    if (input.size() < 3 || !((input[0] >= L'A' && input[0] <= L'Z') || (input[0] >= L'a' && input[0] <= L'z'))
        || input[1] != L':' || input[2] != L'\\'
        || input.find_first_of(L"\"<>|?*\r\n/") != std::wstring::npos
        || input.find(L':', 2) != std::wstring::npos || input.find(L'\0') != std::wstring::npos)
        return Fail(ERROR_BAD_PATHNAME, l10n::Text("error.directory.absolute"));
    for (size_t i = 3; i < input.size(); ++i) {
        if (input[i] < L' ' || ((input[i] == L'.' || input[i] == L' ')
            && (i + 1 == input.size() || input[i + 1] == L'\\')))
            return Fail(ERROR_BAD_PATHNAME, l10n::Text("error.directory.characters"));
    }
    wchar_t full[MAX_PATH]{};
    const DWORD n = GetFullPathNameW(input.c_str(), MAX_PATH, full, nullptr);
    if (!n || n >= MAX_PATH) return Fail(ERROR_FILENAME_EXCED_RANGE, l10n::Text("error.directory.length"));
    output = full;
    while (output.size() > 3 && output.back() == L'\\') output.pop_back();
    if (output.size() + wcslen(LauncherName) + 2 >= MAX_PATH)
        return Fail(ERROR_FILENAME_EXCED_RANGE, l10n::Text("error.directory.shorter"));
    size_t start = 3;
    while (start < output.size()) {
        const size_t end = output.find(L'\\', start);
        const std::wstring part = output.substr(start, end == std::wstring::npos ? end : end - start);
        const std::wstring stem = part.substr(0, part.find(L'.'));
        const bool device = Same(stem, L"CON") || Same(stem, L"PRN") || Same(stem, L"AUX") || Same(stem, L"NUL")
            || (stem.size() == 4 && (Same(stem.substr(0, 3), L"COM") || Same(stem.substr(0, 3), L"LPT"))
                && stem[3] >= L'1' && stem[3] <= L'9');
        if (part.empty() || part.back() == L'.' || part.back() == L' ' || device)
            return Fail(ERROR_BAD_PATHNAME, l10n::Text("error.directory.reserved"));
        if (end == std::wstring::npos) break;
        start = end + 1;
    }
    return {};
}
Result ReadLink(const std::wstring& path, Link& value, bool expandedTarget, LinkIcon* icon) {
    ComPtr<IShellLinkW> link;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link));
    if (FAILED(hr)) return Hr(hr, l10n::Text("error.link.read"));
    ComPtr<IPersistFile> persist;
    if (FAILED(hr = link->QueryInterface(IID_PPV_ARGS(&persist)))
        || FAILED(hr = persist->Load(path.c_str(), STGM_READ))) return Hr(hr, l10n::Format("error.link.readpath", {{"path", path}}));
    std::vector<wchar_t> text(32768);
    Link read;
    auto field = [&](HRESULT status, std::wstring& destination) {
        hr = status;
        if (SUCCEEDED(hr)) destination = text.data();
        text[0] = 0;
        return SUCCEEDED(hr);
    };
    if (!field(link->GetPath(text.data(), 32768, nullptr, expandedTarget ? 0 : SLGP_RAWPATH), read.target)
        || !field(link->GetArguments(text.data(), 32768), read.arguments)
        || !field(link->GetWorkingDirectory(text.data(), 32768), read.directory)
        || !field(link->GetDescription(text.data(), 1024), read.description)) return Hr(hr, l10n::Text("error.link.readproperties"));
    if (icon) {
        *icon = {};
        int index = 0;
        if (link->GetIconLocation(text.data(), static_cast<int>(text.size()), &index) == S_OK)
            *icon = {text.data(), index};
    }
    value = std::move(read);
    return {};
}
Result Load(const Paths& paths, Loaded& loaded) {
    loaded = {};
    loaded.options.directory = paths.defaultDirectory;
    Link saved;
    if (Exists(paths.settingsLink)) {
        Result result = ReadLink(paths.settingsLink, saved);
        if (!result) return result;
        if (!Owned(saved)) return Conflict(l10n::Text("error.settings.conflict"), Parent(paths.settingsLink));
        if (!Same(Join(Parent(saved.target), LauncherName), saved.target))
            return Fail(ERROR_INVALID_DATA, l10n::Text("error.settings.target"));
        loaded.managedTarget = saved.target;
        loaded.options.directory = Parent(saved.target);
        loaded.hasSettings = true;
    }
    // Prefer the current name's arguments when both generations are present.
    for (const auto* path : {&paths.legacyStartupLink, &paths.startupLink}) {
        if (path->empty() || !Exists(*path)) continue;
        Link startup;
        Result result = ReadLink(*path, startup);
        if (!result) return result;
        if (Owned(startup) && Same(startup.target, loaded.managedTarget)) {
            loaded.options.startup = true;
            saved.arguments = startup.arguments;
        } else if (path == &paths.startupLink) {
            loaded.notice = l10n::Text("setup.notice.startupconflict");
        }
    }
    // Presence of our link records the choice; do not import desktop arguments over saved settings.
    for (const auto* path : {&paths.legacyDesktopLink, &paths.desktopLink}) {
        Link desktop;
        if (!path->empty() && Exists(*path) && ReadLink(*path, desktop)
            && Owned(desktop) && Same(desktop.target, loaded.managedTarget))
            loaded.options.desktop = true;
    }
    if (loaded.hasSettings) {
        Result parsed = ParseArguments(saved.arguments, loaded.options);
        loaded.validArguments = static_cast<bool>(parsed);
        if (!parsed) loaded.notice = parsed.message;
        const DWORD a = GetFileAttributesW(loaded.managedTarget.c_str());
        loaded.deployed = a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
        if (loaded.deployed) loaded.deployedVersion = automation::ReadAppVersion(loaded.managedTarget, true);
    } else {
        // These are form defaults, not a statement that Windows startup is already configured.
        loaded.options.startup = true;
        loaded.options.silent = true;
    }
    return {};
}
static Result ApplyFiles(const Paths& paths, const Options& options, Payload payload, std::wstring& deployedTarget, bool preflight) {
    deployedTarget.clear();
    std::wstring directory, arguments;
    Result result = NormalizeDirectory(options.directory, directory);
    if (!result) return result;
    result = BuildArguments(options, arguments);
    if (!result) return result;
    if (!payload.data || payload.size < sizeof(IMAGE_DOS_HEADER) || payload.size > 16 * 1024 * 1024)
        return Fail(ERROR_INVALID_DATA, l10n::Text("error.payload.incomplete"));
    IMAGE_DOS_HEADER dos{};
    CopyMemory(&dos, payload.data, sizeof(dos));
    if (dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < 0
        || static_cast<size_t>(dos.e_lfanew) + sizeof(IMAGE_NT_HEADERS64) > payload.size)
        return Fail(ERROR_BAD_EXE_FORMAT, l10n::Text("error.payload.format"));
    IMAGE_NT_HEADERS64 nt{};
    CopyMemory(&nt, payload.data + dos.e_lfanew, sizeof(nt));
    if (nt.Signature != IMAGE_NT_SIGNATURE || nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64
        || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        return Fail(ERROR_BAD_EXE_FORMAT, l10n::Text("error.payload.architecture"));
    Loaded previous;
    result = Load(paths, previous);
    if (!result) return result;
    std::wstring target = Join(directory, LauncherName);
    Snapshot oldExe, oldSettings, oldStartup, oldDesktop, oldEnv, oldDiscovery, legacyStartup, legacyDesktop;
    const auto discoveryPath = Join(paths.defaultDirectory, automation::contract::DiscoveryFile);
    const bool touchDesktop = options.desktop || previous.options.desktop;
    const bool touchEnv = options.proxy && options.codexEnv;
    auto readLegacy = [&](const std::wstring& path, Snapshot& snapshot) -> Result {
        if (path.empty() || previous.managedTarget.empty()) return {};
        Snapshot candidate;
        Result read = ReadFileBytes(path, candidate); if (!read || !candidate.exists) return read;
        Link link; read = ReadLink(path, link); if (!read) return read;
        if (Owned(link) && Same(link.target, previous.managedTarget)) {
            automation::CheckPlainPath(path);
            snapshot = std::move(candidate);
        }
        return {};
    };
    try {
        for (const auto& path : {target, paths.settingsLink, paths.startupLink, discoveryPath}) automation::CheckPlainPath(path);
        if (touchDesktop && !paths.desktopLink.empty()) automation::CheckPlainPath(paths.desktopLink);
        if (touchEnv && !paths.codexEnvFile.empty()) automation::CheckPlainPath(paths.codexEnvFile);
        result = readLegacy(paths.legacyStartupLink, legacyStartup); if (!result) return result;
        result = readLegacy(paths.legacyDesktopLink, legacyDesktop); if (!result) return result;
    } catch (const automation::Invalid&) { return Fail(ERROR_BAD_PATHNAME, l10n::Text("error.directory.invalid")); }
    catch (const automation::IoError& e) { return Fail(e.code, l10n::Text("error.directory.check")); }
    result = ReadFileBytes(discoveryPath, oldDiscovery); if (!result) return result;
    if (oldDiscovery.exists) {
        try {
            const auto record = automation::ParseJson({reinterpret_cast<const char*>(oldDiscovery.data.data()), oldDiscovery.data.size()});
            if (record.At(L"schemaVersion").Integer(0, MAXDWORD) != automation::contract::DiscoverySchema || record.At(L"managedBy").String() != automation::contract::ManagedBy)
                return Conflict(l10n::Text("error.discovery.owner"), paths.defaultDirectory);
        } catch (...) { return Conflict(l10n::Text("error.discovery.read"), paths.defaultDirectory); }
    }
    std::vector<BYTE> envBytes;
    if (touchEnv) {
        if (paths.codexEnvFile.empty()) return Fail(ERROR_PATH_NOT_FOUND, l10n::Text("error.paths.env"));
        result = ReadFileBytes(paths.codexEnvFile, oldEnv, true); if (!result) return result;
        result = MergeCodexEnv(oldEnv.data, options, envBytes); if (!result) return result;
    }
    const bool managedTarget = Same(target, previous.managedTarget);
    FileMatch existingExe;
    result = CompareLauncher(target, payload, existingExe, managedTarget && !preflight ? &oldExe : nullptr);
    if (!result) return result;
    result = ReadFileBytes(paths.settingsLink, oldSettings); if (!result) return result;
    result = ReadFileBytes(paths.startupLink, oldStartup); if (!result) return result;
    if (touchDesktop) {
        if (paths.desktopLink.empty()) return Fail(ERROR_PATH_NOT_FOUND, l10n::Text("error.paths.desktop"));
        result = ReadFileBytes(paths.desktopLink, oldDesktop); if (!result) return result;
        if (oldDesktop.exists) {
            Link existing;
            if (!ReadLink(paths.desktopLink, existing) || !Owned(existing)
                || !Same(existing.target, previous.managedTarget))
                return Conflict(l10n::Text("error.desktop.conflict"), Parent(paths.desktopLink));
        }
    }
    if (existingExe == FileMatch::Different && !managedTarget)
        return Conflict(l10n::Text("error.deployment.conflict"), directory);
    if (oldStartup.exists) {
        Link existing;
        result = ReadLink(paths.startupLink, existing); if (!result) return result;
        if (!Owned(existing) || !Same(existing.target, previous.managedTarget))
            return Conflict(l10n::Text("error.startup.conflict"), Parent(paths.startupLink));
    }
    if (preflight) return {};
    // Prepare immutable data and fallback messages before the first file is changed.
    // The embedded EXE stays in its resource; no second payload-sized buffer is needed.
    const Link value{ target, arguments, directory, Marker };
    const auto record = automation::MakeDiscoveryRecord(target, automation::FromUtf8(APP_VERSION_TEXT));
    const auto text = automation::WriteJson(record);
    const std::vector<BYTE> discoveryBytes(text.begin(), text.end());
    Result exceptionFailure{ERROR_GEN_FAILURE, l10n::Text("error.deployment.interrupted")};
    std::wstring restoredFallback = exceptionFailure.message + l10n::Text("error.rollback.complete");
    std::wstring incompleteFallback = exceptionFailure.message + l10n::Text("error.rollback.exception");
    const auto desktopDirectory = touchDesktop ? Parent(paths.desktopLink) : std::wstring{};
    const auto legacyDesktopDirectory = legacyDesktop.exists ? Parent(paths.legacyDesktopLink) : std::wstring{};
    result = EnsureDirectory(directory); if (!result) return result;
    result = EnsureDirectory(Parent(paths.settingsLink)); if (!result) return result;
    if (options.startup) { result = EnsureDirectory(Parent(paths.startupLink)); if (!result) return result; }
    if (options.desktop) { result = EnsureDirectory(desktopDirectory); if (!result) return result; }
    if (touchEnv) { result = EnsureDirectory(Parent(paths.codexEnvFile)); if (!result) return result; }
    bool exeChanged = false, settingsChanged = false, startupChanged = false, desktopChanged = false, discoveryChanged = false;
    EnvChange envChange;
    bool legacyStartupRemoved = false, legacyDesktopRemoved = false;
    auto notifyDesktopChanges = [&](bool committed) noexcept {
        // Atomic .tmp -> .lnk replacement alone can leave Explorer's desktop view stale.
        // Notify only after commit/rollback, with no allocations or wait for Explorer.
        constexpr UINT flags = SHCNF_PATHW | SHCNF_FLUSHNOWAIT;
        if (committed) {
            if (desktopChanged) {
                LONG event = SHCNE_DELETE;
                if (options.desktop) event = oldDesktop.exists ? SHCNE_UPDATEITEM : SHCNE_CREATE;
                SHChangeNotify(event, flags, paths.desktopLink.c_str(), nullptr);
            }
            if (legacyDesktopRemoved) SHChangeNotify(SHCNE_DELETE, flags, paths.legacyDesktopLink.c_str(), nullptr);
        }
        // A directory refresh also covers rollback, including a partial restore.
        if (desktopChanged) SHChangeNotify(SHCNE_UPDATEDIR, flags, desktopDirectory.c_str(), nullptr);
        if (legacyDesktopRemoved && (!desktopChanged || !Same(desktopDirectory, legacyDesktopDirectory)))
            SHChangeNotify(SHCNE_UPDATEDIR, flags, legacyDesktopDirectory.c_str(), nullptr);
    };
    auto rollback = [&](Result failure) noexcept {
        bool incomplete = false, detailsUnavailable = false;
        std::wstring restoreErrors;
        auto restore = [&](bool changed, const std::wstring& path, const Snapshot& before, bool replace = true) noexcept {
            if (!changed) return;
            try {
                const Result restored = Restore(path, before, replace);
                if (!restored) { incomplete = true; restoreErrors += L"\n" + restored.message; }
            } catch (...) { incomplete = true; detailsUnavailable = true; }
        };
        // A failed restore or diagnostic allocation must not skip the remaining files.
        restore(discoveryChanged, discoveryPath, oldDiscovery);
        if (envChange.installed || !envChange.backupPath.empty()) {
            try {
                const auto restored = envChange.Undo(paths.codexEnvFile, envBytes);
                if (!restored) { incomplete = true; restoreErrors += L"\n" + restored.message; }
            } catch (...) { incomplete = true; detailsUnavailable = true; }
        }
        restore(legacyDesktopRemoved, paths.legacyDesktopLink, legacyDesktop, false);
        restore(legacyStartupRemoved, paths.legacyStartupLink, legacyStartup, false);
        // Removed links have no replacement to undo. Do not overwrite a file created meanwhile.
        restore(desktopChanged, paths.desktopLink, oldDesktop, options.desktop);
        restore(startupChanged, paths.startupLink, oldStartup, options.startup);
        restore(settingsChanged, paths.settingsLink, oldSettings);
        restore(exeChanged, target, oldExe);
        notifyDesktopChanges(false);
        failure.commit = incomplete ? CommitState::Incomplete : CommitState::RolledBack;
        try {
            if (detailsUnavailable) failure.message += l10n::Text("error.rollback.exception");
            else failure.message += incomplete ? l10n::Format("error.rollback.incomplete", {{"errors", restoreErrors}})
                : l10n::Text("error.rollback.complete");
        } catch (...) { failure.message.swap(incomplete ? incompleteFallback : restoredFallback); }
        return failure;
    };
    auto removeShortcut = [&](const std::wstring& path, const Snapshot& before, bool& removed, const char* errorKey) -> Result {
        if (!before.exists) return {};
        try {
            automation::RemoveUnchangedFile(path, std::string_view(
                reinterpret_cast<const char*>(before.data.data()), before.data.size()));
        } catch (const automation::IoError& e) {
            return Fail(e.code, l10n::Text(e.code == ERROR_RETRY ? "error.link.concurrent" : errorKey));
        }
        removed = true;
        return {};
    };
    try {
        if (existingExe != FileMatch::Identical) {
            result = WriteAtomicBytes(target, payload.data, payload.size, oldExe.exists); if (!result) return rollback(std::move(result));
            exeChanged = true;
        }
        FileMatch written;
        result = CompareLauncher(target, payload, written);
        if (!result) return rollback(std::move(result));
        if (written != FileMatch::Identical) return rollback(Fail(ERROR_CRC, l10n::Text("error.deployment.verify")));
        result = WriteLink(paths.settingsLink, value, oldSettings.exists); if (!result) return rollback(std::move(result));
        settingsChanged = true;
        if (options.startup) {
            result = WriteLink(paths.startupLink, value, oldStartup.exists); if (!result) return rollback(std::move(result));
            startupChanged = true;
        } else if (oldStartup.exists) {
            result = removeShortcut(paths.startupLink, oldStartup, startupChanged, "error.startup.disable");
            if (!result) return rollback(std::move(result));
        }
        if (options.desktop) {
            result = WriteLink(paths.desktopLink, value, oldDesktop.exists); if (!result) return rollback(std::move(result));
            desktopChanged = true;
        } else if (previous.options.desktop && oldDesktop.exists) {
            result = removeShortcut(paths.desktopLink, oldDesktop, desktopChanged, "error.desktop.remove");
            if (!result) return rollback(std::move(result));
        }
        result = removeShortcut(paths.legacyStartupLink, legacyStartup, legacyStartupRemoved, "error.startup.disable");
        if (!result) return rollback(std::move(result));
        result = removeShortcut(paths.legacyDesktopLink, legacyDesktop, legacyDesktopRemoved, "error.desktop.remove");
        if (!result) return rollback(std::move(result));
        Loaded verify;
        result = Load(paths, verify);
        if (!result) return rollback(std::move(result));
        if (!verify.deployed || verify.options.startup != options.startup || verify.options.desktop != options.desktop
            || !Same(verify.managedTarget, target))
            return rollback(Fail(ERROR_INVALID_DATA, l10n::Text("error.settings.verify")));
        Snapshot check;
        if (touchEnv && (!oldEnv.exists || oldEnv.data != envBytes)) {
            result = WriteEnv(paths.codexEnvFile, oldEnv, envBytes, envChange); if (!result) return rollback(std::move(result));
            automation::Handle published(CreateFileW(paths.codexEnvFile.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
            if (!published) return rollback(Fail(GetLastError(), l10n::Text("error.env.verify")));
            try { automation::CheckFileContents(published.value, Bytes(envBytes), &envChange.revision); }
            catch (const automation::IoError& e) {
                published.Reset();
                return rollback(Fail(e.code, l10n::Text("error.env.verify")));
            }
        }
        result = ReadFileBytes(discoveryPath, check); if (!result) return rollback(std::move(result));
        if (check.exists != oldDiscovery.exists || check.data != oldDiscovery.data)
            return rollback(Fail(ERROR_RETRY, l10n::Text("error.discovery.concurrent")));
        if (!oldDiscovery.exists || discoveryBytes != oldDiscovery.data) {
            result = WriteAtomic(discoveryPath, discoveryBytes, oldDiscovery.exists); if (!result) return rollback(std::move(result));
            discoveryChanged = true;
            result = ReadFileBytes(discoveryPath, check); if (!result) return rollback(std::move(result));
            if (!check.exists || check.data != discoveryBytes) return rollback(Fail(ERROR_CRC, l10n::Text("error.discovery.verify")));
        }
        result = envChange.DiscardOriginal();
        if (!result) return rollback(std::move(result));
    } catch (const std::bad_alloc&) {
        exceptionFailure.code = ERROR_NOT_ENOUGH_MEMORY;
        return rollback(std::move(exceptionFailure));
    } catch (...) { return rollback(std::move(exceptionFailure)); }
    notifyDesktopChanges(true);
    deployedTarget.swap(target); // Publishing a committed result must not allocate.
    Result success; success.commit = CommitState::Committed; return success;
}
Result Apply(const Paths& paths, const Options& options, Payload payload, std::wstring& deployedTarget, bool isolated) {
    automation::UserLock lock;
    const DWORD error = lock.AcquireForSetup(paths, isolated);
    if (error) return Fail(error, l10n::Text("error.busy"));
    return ApplyFiles(paths, options, payload, deployedTarget, false);
}
Result Preflight(const Paths& paths, const Options& options, Payload payload) {
    std::wstring unused;
    return ApplyFiles(paths, options, payload, unused, true);
}
} // namespace setup
