// SPDX-License-Identifier: GPL-3.0-only
#include "SetupCore.h"
#include "CodexEnv.h"
#include "StartupApps.h"
#include <shlobj.h>
#include <shlwapi.h>
#include <utility>

namespace setup {
namespace {
template<class T> struct Com {
    T* p = nullptr;
    ~Com() { if (p) p->Release(); }
    T* operator->() const { return p; }
};
struct Handle {
    HANDLE h = INVALID_HANDLE_VALUE;
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
    return Fail(static_cast<DWORD>(error), L"无法创建目录：" + path);
}
struct Snapshot { bool exists = false; std::vector<BYTE> data; };
std::wstring ReadFileVersion(const std::wstring& path) {
    // Read metadata only. Never start the deployed executable to ask for its version.
    DWORD ignored = 0;
    const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &ignored);
    if (!size || size > 1024 * 1024) return {};
    std::vector<BYTE> data(size);
    if (!GetFileVersionInfoW(path.c_str(), 0, size, data.data())) return {};
    LPVOID value = nullptr;
    UINT length = 0;
    if (!VerQueryValueW(data.data(), L"\\", &value, &length) || !value || length < sizeof(VS_FIXEDFILEINFO)) return {};
    VS_FIXEDFILEINFO info{};
    CopyMemory(&info, value, sizeof(info));
    if (info.dwSignature != VS_FFI_SIGNATURE || info.dwFileType != VFT_APP
        || (!info.dwFileVersionMS && !info.dwFileVersionLS)) return {};
    std::wstring version = std::to_wstring(HIWORD(info.dwFileVersionMS)) + L"."
        + std::to_wstring(LOWORD(info.dwFileVersionMS)) + L"." + std::to_wstring(HIWORD(info.dwFileVersionLS));
    if (LOWORD(info.dwFileVersionLS)) version += L"." + std::to_wstring(LOWORD(info.dwFileVersionLS));
    return version;
}
Result ReadFileBytes(const std::wstring& path, Snapshot& file) {
    file = {};
    Handle handle;
    handle.h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle.h == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        return IsMissing(error) ? Result{} : Fail(error, L"无法读取：" + path);
    }
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(handle.h, &size)) return Fail(GetLastError(), L"无法读取文件大小。");
    if (size.QuadPart < 0 || size.QuadPart > 32 * 1024 * 1024)
        return Fail(ERROR_FILE_TOO_LARGE, L"文件超过本工具的处理范围：" + path);
    file.data.resize(static_cast<size_t>(size.QuadPart));
    DWORD count = 0;
    if (!ReadFile(handle.h, file.data.data(), static_cast<DWORD>(file.data.size()), &count, nullptr))
        return Fail(GetLastError(), L"无法读取文件内容：" + path);
    if (count != file.data.size()) return Fail(ERROR_READ_FAULT, L"文件读取不完整：" + path);
    file.exists = true;
    return {};
}
// Each temporary object owns exactly one newly-created, explicit file path.
struct TempFile {
    std::wstring path;
    ~TempFile() { if (!path.empty()) DeleteFileW(path.c_str()); }
};
Result NewTemp(const std::wstring& target, TempFile& temp) {
    wchar_t name[MAX_PATH]{};
    if (!GetTempFileNameW(Parent(target).c_str(), L"cgl", 0, name))
        return Fail(GetLastError(), L"无法在目标目录创建临时文件。");
    temp.path = name;
    return {};
}
Result CommitTemp(TempFile& temp, const std::wstring& target, bool replace = true) {
    if (!MoveFileExW(temp.path.c_str(), target.c_str(), (replace ? MOVEFILE_REPLACE_EXISTING : 0) | MOVEFILE_WRITE_THROUGH)) {
        const DWORD error = GetLastError();
        if (!replace && (error == ERROR_ALREADY_EXISTS || error == ERROR_FILE_EXISTS))
            return Conflict(L"保存期间出现同名文件，未覆盖：" + target, Parent(target));
        return Fail(error, L"无法保存文件，可能没有权限或文件正在使用：" + target);
    }
    temp.path.clear();
    return {};
}
Result WriteAtomic(const std::wstring& path, const std::vector<BYTE>& data, bool replace = true) {
    TempFile temp;
    Result result = NewTemp(path, temp);
    if (!result) return result;
    {
        Handle handle;
        handle.h = CreateFileW(temp.path.c_str(), GENERIC_WRITE, 0, nullptr, TRUNCATE_EXISTING,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle.h == INVALID_HANDLE_VALUE) return Fail(GetLastError(), L"无法打开临时文件。");
        DWORD written = 0;
        if (!WriteFile(handle.h, data.data(), static_cast<DWORD>(data.size()), &written, nullptr)
            || written != data.size() || !FlushFileBuffers(handle.h))
            return Fail(GetLastError(), L"无法完整写入文件。");
    }
    return CommitTemp(temp, path, replace);
}
Result Restore(const std::wstring& path, const Snapshot& snapshot) {
    if (snapshot.exists) return WriteAtomic(path, snapshot.data);
    if (DeleteFileW(path.c_str()) || IsMissing(GetLastError())) return {};
    return Fail(GetLastError(), L"无法撤销新建文件：" + path);
}
Result WriteLink(const std::wstring& path, const Link& value, bool replace = true) {
    Com<IShellLinkW> link;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link.p));
    if (FAILED(hr)) return Hr(hr, L"无法创建快捷方式对象。");
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
        || FAILED(hr = link->SetShowCmd(SW_SHOWNORMAL))) return Hr(hr, L"无法设置快捷方式属性。");
    Com<IPersistFile> persist;
    if (FAILED(hr = link->QueryInterface(IID_PPV_ARGS(&persist.p)))) return Hr(hr, L"无法保存快捷方式。");
    TempFile temp;
    Result result = NewTemp(path, temp);
    if (!result) return result;
    if (FAILED(hr = persist->Save(temp.path.c_str(), TRUE))) return Hr(hr, L"写入快捷方式失败。");
    Link read;
    result = ReadLink(temp.path, read);
    if (!result) return result;
    if (!Same(read.target, value.target) || read.arguments != value.arguments
        || !Same(read.directory, value.directory) || read.description != value.description)
        return Fail(ERROR_INVALID_DATA, L"快捷方式回读结果不一致，已停止保存。");
    return CommitTemp(temp, path, replace);
}
} // namespace

std::wstring ErrorText(DWORD error) {
    PWSTR message = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error, 0, reinterpret_cast<PWSTR>(&message), 0, nullptr);
    std::wstring result = message ? message : L"Windows 未提供详细说明。";
    if (message) LocalFree(message);
    while (!result.empty() && (result.back() == L'\r' || result.back() == L'\n')) result.pop_back();
    return result + L"（错误码 " + std::to_wstring(error) + L"）";
}
std::wstring Join(const std::wstring& directory, const std::wstring& file) {
    return directory + (!directory.empty() && directory.back() == L'\\' ? L"" : L"\\") + file;
}
Result GetPaths(Paths& paths) {
    PWSTR local = nullptr, startup = nullptr;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local);
    if (FAILED(hr)) return Hr(hr, L"无法获取用户程序目录。");
    paths.defaultDirectory = Join(local, companions::Root);
    CoTaskMemFree(local);
    paths.settingsLink = Join(paths.defaultDirectory, L"Launcher settings.lnk");
    hr = SHGetKnownFolderPath(FOLDERID_Startup, 0, nullptr, &startup);
    if (FAILED(hr)) return Hr(hr, L"无法获取当前用户的启动文件夹。");
    paths.startupLink = Join(startup, LinkName);
    CoTaskMemFree(startup);
    PWSTR desktop = nullptr;
    hr = SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &desktop);
    // The optional desktop entry must not prevent deployment when the desktop is unavailable.
    paths.desktopLink = SUCCEEDED(hr) ? Join(desktop, LinkName) : L"";
    CoTaskMemFree(desktop);
    PWSTR profile = nullptr;
    hr = SHGetKnownFolderPath(FOLDERID_Profile, 0, nullptr, &profile);
    paths.codexEnvFile = SUCCEEDED(hr) ? Join(Join(profile, L".codex"), L".env") : L"";
    CoTaskMemFree(profile);
    return {};
}
Result EnsureCompanionFolder(const Paths& paths, std::wstring& folder) {
    if (paths.defaultDirectory.empty()) return Fail(ERROR_PATH_NOT_FOUND, L"无法获取附加程序目录。");
    folder = Join(paths.defaultDirectory, companions::Folder);
    Result result = EnsureDirectory(folder);
    if (!result) return result;
    if (GetFileAttributesW(folder.c_str()) & FILE_ATTRIBUTE_REPARSE_POINT)
        return Fail(ERROR_BAD_PATHNAME, L"启动目录不能是目录链接。");
    return {};
}
Result AddCompanionProgram(const Paths& paths, const std::wstring& program) {
    const std::wstring name = program.substr(program.find_last_of(L'\\') + 1);
    auto targetKind = companions::FileKind(name.c_str());
    const bool importing = targetKind == companions::Kind::Link;
    if (!companions::LocalPath(program.c_str()) || (!importing && !companions::ValidTarget(program.c_str(), L"", L"")))
        return Fail(ERROR_BAD_PATHNAME, L"请选择文件列表中支持的本机程序、脚本或快捷方式，不要选择启动器或配置器。");
    DWORD attrs = GetFileAttributesW(program.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY))
        return Fail(ERROR_FILE_NOT_FOUND, L"找不到所选程序。");
    if (importing) {
        Link source;
        Result read = ReadLink(program, source, true);
        if (!read) return read;
        if (companions::FileKind(source.target.c_str()) == companions::Kind::Batch && !source.arguments.empty())
            return Fail(ERROR_INVALID_PARAMETER, L"批处理快捷方式不支持额外参数。请将所需选项写在 BAT 或 CMD 文件中。");
        if (!companions::ValidTarget(source.target.c_str(), source.arguments.c_str(), source.directory.c_str()))
            return Fail(ERROR_BAD_PATHNAME, L"快捷方式必须指向受支持的本机程序或脚本，工作目录也必须在本机。");
        targetKind = companions::FileKind(source.target.c_str());
        DWORD targetAttrs = GetFileAttributesW(source.target.c_str());
        if (targetAttrs == INVALID_FILE_ATTRIBUTES || (targetAttrs & FILE_ATTRIBUTE_DIRECTORY))
            return Fail(ERROR_FILE_NOT_FOUND, L"快捷方式指向的文件不存在。");
    }
    if (targetKind != companions::Kind::Exe && companions::FindHost(targetKind).empty()) {
        if (PCWSTR host = companions::PathHost(targetKind))
            return Fail(ERROR_FILE_NOT_FOUND, std::wstring(L"在 PATH 中找不到 ") + host
                + L"。请安装对应运行环境后重新打开配置器，或选择指向解释器的快捷方式。");
        return Fail(ERROR_FILE_NOT_FOUND, L"找不到此脚本需要的 Windows 脚本运行程序。");
    }
    std::wstring folder;
    Result result = EnsureCompanionFolder(paths, folder);
    if (!result) return result;
    if (Same(Parent(program), folder)) return {}; // Already in the dispatch list.
    const std::wstring destination = Join(folder, name.substr(0, name.find_last_of(L'.')) + L".lnk");
    if (Exists(destination)) {
        if (importing) {
            Snapshot a, b;
            if (ReadFileBytes(program, a) && ReadFileBytes(destination, b) && a.exists && b.exists && a.data == b.data) return {};
        } else {
            Link existing;
            if (ReadLink(destination, existing) && Same(existing.target, program)) return {};
        }
        return Conflict(L"启动目录已有同名快捷方式。请先移走或重命名，原文件未修改。", folder);
    }
    if (importing) {
        TempFile temp;
        result = NewTemp(destination, temp);
        if (!result) return result;
        if (!CopyFileW(program.c_str(), temp.path.c_str(), FALSE)) return Fail(GetLastError(), L"无法复制所选快捷方式。");
        return CommitTemp(temp, destination, false);
    }
    return WriteLink(destination, {program, L"", Parent(program), L"ChatGPT 启动时打开"}, false);
}
Result NormalizeDirectory(const std::wstring& input, std::wstring& output) {
    if (input.size() < 3 || !((input[0] >= L'A' && input[0] <= L'Z') || (input[0] >= L'a' && input[0] <= L'z'))
        || input[1] != L':' || input[2] != L'\\'
        || input.find_first_of(L"\"<>|?*\r\n/") != std::wstring::npos
        || input.find(L':', 2) != std::wstring::npos || input.find(L'\0') != std::wstring::npos)
        return Fail(ERROR_BAD_PATHNAME, L"请选择本机磁盘上的完整目录，例如 D:\\Apps\\ChatGPTLauncher。");
    for (size_t i = 3; i < input.size(); ++i) {
        if (input[i] < L' ' || ((input[i] == L'.' || input[i] == L' ')
            && (i + 1 == input.size() || input[i + 1] == L'\\')))
            return Fail(ERROR_BAD_PATHNAME, L"目录名称不能以空格或句点结尾，也不能含控制字符。");
    }
    wchar_t full[MAX_PATH]{};
    const DWORD n = GetFullPathNameW(input.c_str(), MAX_PATH, full, nullptr);
    if (!n || n >= MAX_PATH) return Fail(ERROR_FILENAME_EXCED_RANGE, L"部署路径过长。");
    output = full;
    while (output.size() > 3 && output.back() == L'\\') output.pop_back();
    if (output.size() + wcslen(LauncherName) + 2 >= MAX_PATH)
        return Fail(ERROR_FILENAME_EXCED_RANGE, L"部署路径过长，请选择更短的目录。");
    size_t start = 3;
    while (start < output.size()) {
        const size_t end = output.find(L'\\', start);
        const std::wstring part = output.substr(start, end == std::wstring::npos ? end : end - start);
        const std::wstring stem = part.substr(0, part.find(L'.'));
        const bool device = Same(stem, L"CON") || Same(stem, L"PRN") || Same(stem, L"AUX") || Same(stem, L"NUL")
            || (stem.size() == 4 && (Same(stem.substr(0, 3), L"COM") || Same(stem.substr(0, 3), L"LPT"))
                && stem[3] >= L'1' && stem[3] <= L'9');
        if (part.empty() || part.back() == L'.' || part.back() == L' ' || device)
            return Fail(ERROR_BAD_PATHNAME, L"部署目录包含 Windows 不支持的名称。");
        if (end == std::wstring::npos) break;
        start = end + 1;
    }
    return {};
}
Result ReadLink(const std::wstring& path, Link& value, bool expandedTarget) {
    Com<IShellLinkW> link;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link.p));
    if (FAILED(hr)) return Hr(hr, L"无法读取快捷方式。");
    Com<IPersistFile> persist;
    if (FAILED(hr = link->QueryInterface(IID_PPV_ARGS(&persist.p)))
        || FAILED(hr = persist->Load(path.c_str(), STGM_READ))) return Hr(hr, L"无法读取快捷方式：" + path);
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
        || !field(link->GetDescription(text.data(), 1024), read.description)) return Hr(hr, L"无法读取快捷方式属性。");
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
        if (!Owned(saved)) return Conflict(L"配置记录被其他文件占用，未做任何修改。", Parent(paths.settingsLink));
        if (!Same(Join(Parent(saved.target), LauncherName), saved.target))
            return Fail(ERROR_INVALID_DATA, L"配置记录中的启动器路径不正确。");
        loaded.managedTarget = saved.target;
        loaded.options.directory = Parent(saved.target);
        loaded.hasSettings = true;
    }
    Link startup;
    if (Exists(paths.startupLink)) {
        Result result = ReadLink(paths.startupLink, startup);
        if (!result) return result;
        if (Owned(startup) && Same(startup.target, loaded.managedTarget)) {
            loaded.options.startup = true;
            saved.arguments = startup.arguments;
        } else {
            loaded.notice = L"启动文件夹中有同名快捷方式。保存前请手动移走或重命名，工具不会覆盖它。";
        }
    }
    // Presence of our link records the choice; do not import desktop arguments over saved settings.
    Link desktop;
    if (Exists(paths.desktopLink) && ReadLink(paths.desktopLink, desktop)
        && Owned(desktop) && Same(desktop.target, loaded.managedTarget))
        loaded.options.desktop = true;
    if (loaded.hasSettings) {
        Result parsed = ParseArguments(saved.arguments, loaded.options);
        if (!parsed) loaded.notice = parsed.message;
        const DWORD a = GetFileAttributesW(loaded.managedTarget.c_str());
        loaded.deployed = a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
        if (loaded.deployed) loaded.deployedVersion = ReadFileVersion(loaded.managedTarget);
    } else {
        // These are form defaults, not a statement that Windows startup is already configured.
        loaded.options.startup = true;
        loaded.options.silent = true;
    }
    return {};
}
Result Apply(const Paths& paths, const Options& options, Payload payload, std::wstring& deployedTarget) {
    deployedTarget.clear();
    std::wstring directory, arguments;
    Result result = NormalizeDirectory(options.directory, directory);
    if (!result) return result;
    result = BuildArguments(options, arguments);
    if (!result) return result;
    if (!payload.data || payload.size < sizeof(IMAGE_DOS_HEADER) || payload.size > 16 * 1024 * 1024)
        return Fail(ERROR_INVALID_DATA, L"内置启动器资源不完整。");
    IMAGE_DOS_HEADER dos{};
    CopyMemory(&dos, payload.data, sizeof(dos));
    if (dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < 0
        || static_cast<size_t>(dos.e_lfanew) + sizeof(IMAGE_NT_HEADERS64) > payload.size)
        return Fail(ERROR_BAD_EXE_FORMAT, L"内置启动器格式不正确。");
    IMAGE_NT_HEADERS64 nt{};
    CopyMemory(&nt, payload.data + dos.e_lfanew, sizeof(nt));
    if (nt.Signature != IMAGE_NT_SIGNATURE || nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64
        || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        return Fail(ERROR_BAD_EXE_FORMAT, L"内置启动器不是 Windows x64 程序。");
    Loaded previous;
    result = Load(paths, previous);
    if (!result) return result;
    const std::wstring target = Join(directory, LauncherName);
    Snapshot oldExe, oldSettings, oldStartup, oldDesktop, oldEnv;
    std::vector<BYTE> envBytes;
    const bool touchEnv = options.proxy && options.codexEnv;
    if (touchEnv) {
        if (paths.codexEnvFile.empty()) return Fail(ERROR_PATH_NOT_FOUND, L"无法获取 Codex 的 .env 文件位置。");
        const DWORD attributes = GetFileAttributesW(paths.codexEnvFile.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)))
            return Fail(ERROR_BAD_PATHNAME, L"Codex 的 .env 不是普通文件，请手动检查。");
        result = ReadFileBytes(paths.codexEnvFile, oldEnv); if (!result) return result;
        result = MergeCodexEnv(oldEnv.data, options, envBytes); if (!result) return result;
    }
    result = ReadFileBytes(target, oldExe); if (!result) return result;
    result = ReadFileBytes(paths.settingsLink, oldSettings); if (!result) return result;
    result = ReadFileBytes(paths.startupLink, oldStartup); if (!result) return result;
    const bool touchDesktop = options.desktop || previous.options.desktop;
    if (touchDesktop) {
        if (paths.desktopLink.empty()) return Fail(ERROR_PATH_NOT_FOUND, L"无法获取桌面快捷方式的位置。");
        result = ReadFileBytes(paths.desktopLink, oldDesktop); if (!result) return result;
        if (oldDesktop.exists) {
            Link existing;
            if (!ReadLink(paths.desktopLink, existing) || !Owned(existing)
                || !Same(existing.target, previous.managedTarget))
                return Conflict(L"桌面已有未由本工具管理的同名快捷方式。请先手动移走或重命名，原文件未修改。", Parent(paths.desktopLink));
        }
    }
    const std::vector<BYTE> bytes(payload.data, payload.data + payload.size);
    if (oldExe.exists && !Same(target, previous.managedTarget) && oldExe.data != bytes)
        return Conflict(L"目标目录已有未由本工具管理的同名程序。请换一个目录，原文件未修改。", directory);
    if (oldStartup.exists) {
        Link existing;
        result = ReadLink(paths.startupLink, existing); if (!result) return result;
        if (!Owned(existing) || !Same(existing.target, previous.managedTarget))
            return Conflict(L"启动文件夹中已有其他同名快捷方式。请先手动移走或重命名，原文件未修改。", Parent(paths.startupLink));
    }
    result = EnsureDirectory(directory); if (!result) return result;
    result = EnsureDirectory(Parent(paths.settingsLink)); if (!result) return result;
    if (options.startup) { result = EnsureDirectory(Parent(paths.startupLink)); if (!result) return result; }
    if (options.desktop) { result = EnsureDirectory(Parent(paths.desktopLink)); if (!result) return result; }
    if (touchEnv) { result = EnsureDirectory(Parent(paths.codexEnvFile)); if (!result) return result; }
    bool exeChanged = false, settingsChanged = false, startupChanged = false, desktopChanged = false, envChanged = false;
    auto rollback = [&](Result failure) {
        std::wstring restoreErrors;
        auto note = [&](const Result& restored) { if (!restored) restoreErrors += L"\n" + restored.message; };
        if (envChanged) note(Restore(paths.codexEnvFile, oldEnv));
        if (desktopChanged) note(Restore(paths.desktopLink, oldDesktop));
        if (startupChanged) note(Restore(paths.startupLink, oldStartup));
        if (settingsChanged) note(Restore(paths.settingsLink, oldSettings));
        if (exeChanged) note(Restore(target, oldExe));
        failure.message += restoreErrors.empty() ? L"\n本次文件更改已撤销。" : L"\n部分更改未能恢复，请重新运行工具检查：" + restoreErrors;
        return failure;
    };
    if (!oldExe.exists || oldExe.data != bytes) {
        result = WriteAtomic(target, bytes, oldExe.exists); if (!result) return result;
        exeChanged = true;
    }
    Snapshot check;
    result = ReadFileBytes(target, check);
    if (!result) return rollback(result);
    if (!check.exists || check.data != bytes) return rollback(Fail(ERROR_CRC, L"部署后的程序校验失败。"));
    const Link value{ target, arguments, directory, Marker };
    result = WriteLink(paths.settingsLink, value, oldSettings.exists); if (!result) return rollback(result);
    settingsChanged = true;
    if (options.startup) {
        result = WriteLink(paths.startupLink, value, oldStartup.exists); if (!result) return rollback(result);
        startupChanged = true;
    } else if (oldStartup.exists) {
        if (!DeleteFileW(paths.startupLink.c_str())) return rollback(Fail(GetLastError(), L"无法关闭登录自启。"));
        startupChanged = true;
    }
    if (options.desktop) {
        result = WriteLink(paths.desktopLink, value, oldDesktop.exists); if (!result) return rollback(result);
        desktopChanged = true;
    } else if (previous.options.desktop && oldDesktop.exists) {
        if (!DeleteFileW(paths.desktopLink.c_str())) return rollback(Fail(GetLastError(), L"无法移除桌面快捷方式。"));
        desktopChanged = true;
    }
    Loaded verify;
    result = Load(paths, verify);
    if (!result) return rollback(result);
    if (!verify.deployed || verify.options.startup != options.startup || verify.options.desktop != options.desktop
        || !Same(verify.managedTarget, target))
        return rollback(Fail(ERROR_INVALID_DATA, L"保存后的配置校验失败。"));
    if (touchEnv && (!oldEnv.exists || oldEnv.data != envBytes)) {
        result = ReadFileBytes(paths.codexEnvFile, check); if (!result) return rollback(result);
        if (check.exists != oldEnv.exists || check.data != oldEnv.data)
            return rollback(Fail(ERROR_RETRY, L"保存期间 Codex 的 .env 被其他程序更改，请重试。"));
        result = WriteAtomic(paths.codexEnvFile, envBytes, oldEnv.exists); if (!result) return rollback(result);
        envChanged = true;
        result = ReadFileBytes(paths.codexEnvFile, check); if (!result) return rollback(result);
        if (!check.exists || check.data != envBytes) return rollback(Fail(ERROR_CRC, L"Codex 的 .env 保存校验失败。"));
    }
    deployedTarget = target;
    return {};
}
} // namespace setup
