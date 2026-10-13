// SPDX-License-Identifier: GPL-3.0-only
#include "SetupCore.h"
#include <shlobj.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <algorithm>
#include <cwctype>
#include <memory>

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
Result CommitTemp(TempFile& temp, const std::wstring& target) {
    if (!MoveFileExW(temp.path.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        return Fail(GetLastError(), L"无法保存文件，可能没有权限或文件正在使用：" + target);
    temp.path.clear();
    return {};
}
Result WriteAtomic(const std::wstring& path, const std::vector<BYTE>& data) {
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
    return CommitTemp(temp, path);
}
Result Restore(const std::wstring& path, const Snapshot& snapshot) {
    if (snapshot.exists) return WriteAtomic(path, snapshot.data);
    if (DeleteFileW(path.c_str()) || IsMissing(GetLastError())) return {};
    return Fail(GetLastError(), L"无法撤销新建文件：" + path);
}
Result WriteLink(const std::wstring& path, const Link& value) {
    Com<IShellLinkW> link;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link.p));
    if (FAILED(hr)) return Hr(hr, L"无法创建快捷方式对象。");
    if (FAILED(hr = link->SetPath(value.target.c_str()))
        || FAILED(hr = link->SetArguments(value.arguments.c_str()))
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
    return CommitTemp(temp, path);
}
struct Token { std::wstring value, raw; size_t start = 0, end = 0; };
Result Tokens(const std::wstring& arguments, std::vector<Token>& tokens) {
    tokens.clear();
    if (arguments.find_first_of(L"\r\n") != std::wstring::npos
        || arguments.find(L'\0') != std::wstring::npos)
        return Fail(ERROR_INVALID_PARAMETER, L"启动参数不能包含换行或空字符。");
    size_t i = 0;
    while (i < arguments.size()) {
        if (arguments[i] == L' ' || arguments[i] == L'\t') { ++i; continue; }
        const size_t start = i;
        bool quoted = false;
        size_t slashes = 0;
        for (; i < arguments.size(); ++i) {
            const wchar_t c = arguments[i];
            if (!quoted && (c == L' ' || c == L'\t')) break;
            if (c == L'"' && slashes % 2 == 0) quoted = !quoted;
            slashes = c == L'\\' ? slashes + 1 : 0;
        }
        if (quoted) return Fail(ERROR_INVALID_PARAMETER, L"额外启动参数中有未闭合的双引号。");
        const std::wstring raw = arguments.substr(start, i - start);
        int count = 0;
        LPWSTR* argv = CommandLineToArgvW((L"program " + raw).c_str(), &count);
        if (!argv) return Fail(GetLastError(), L"无法解析启动参数。");
        if (count != 2) { LocalFree(argv); return Fail(ERROR_INVALID_DATA, L"启动参数格式无效。"); }
        tokens.push_back({ argv[1], raw, start, i });
        LocalFree(argv);
    }
    return {};
}
int ManagedKind(const std::wstring& arg) {
    const std::wstring key = arg.substr(0, arg.find(L'='));
    if (key == L"--proxy-server") return 1;
    if (key == L"--no-startup-window") return 2;
    if (key == L"--proxy-bypass-list") return 3;
    return 0;
}
void Append(std::wstring& text, const std::wstring& item) {
    if (item.empty()) return;
    if (!text.empty()) text += L' ';
    text += item;
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
    paths.defaultDirectory = Join(local, L"ChatGPTProxyLauncher");
    CoTaskMemFree(local);
    paths.settingsLink = Join(paths.defaultDirectory, L"Launcher settings.lnk");
    hr = SHGetKnownFolderPath(FOLDERID_Startup, 0, nullptr, &startup);
    if (FAILED(hr)) return Hr(hr, L"无法获取当前用户的启动文件夹。");
    paths.startupLink = Join(startup, LinkName);
    CoTaskMemFree(startup);
    return {};
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
std::wstring QuoteArgument(const std::wstring& value) {
    if (!value.empty() && value.find_first_of(L" \t\"") == std::wstring::npos) return value;
    std::wstring out = L"\"";
    size_t slashes = 0;
    for (wchar_t c : value) {
        if (c == L'\\') { ++slashes; continue; }
        out.append(c == L'"' ? slashes * 2 + 1 : slashes, L'\\');
        slashes = 0;
        out += c;
    }
    out.append(slashes * 2, L'\\');
    return out + L'"';
}
Result BuildArguments(const Options& options, std::wstring& arguments) {
    arguments.clear();
    if (options.proxy) {
        if (options.protocol != L"http" && options.protocol != L"https" && options.protocol != L"socks5")
            return Fail(ERROR_INVALID_PARAMETER, L"请选择有效的代理协议。");
        if (options.host.empty() || options.host.find_first_of(L" \t\r\n/\\\"@;=?#") != std::wstring::npos
            || options.host.find(L'\0') != std::wstring::npos)
            return Fail(ERROR_INVALID_PARAMETER, L"代理地址只填写主机名或 IP，不包含协议、端口或账号密码。");
        if (options.host.find(L':') != std::wstring::npos
            && (options.host.front() != L'[' || options.host.back() != L']'))
            return Fail(ERROR_INVALID_PARAMETER, L"IPv6 地址请放在方括号中，例如 [::1]。");
        if (options.port.empty() || options.port.size() > 5
            || options.port.find_first_not_of(L"0123456789") != std::wstring::npos)
            return Fail(ERROR_INVALID_PARAMETER, L"代理端口应为 1 到 65535 的数字。");
        const unsigned long port = wcstoul(options.port.c_str(), nullptr, 10);
        if (!port || port > 65535) return Fail(ERROR_INVALID_PARAMETER, L"代理端口应为 1 到 65535。");
        Append(arguments, QuoteArgument(L"--proxy-server=" + options.protocol + L"://" + options.host + L":" + options.port));
    }
    if (options.silent) Append(arguments, L"--no-startup-window");
    if (!options.bypass.empty()) Append(arguments, QuoteArgument(L"--proxy-bypass-list=" + options.bypass));
    Append(arguments, options.extra);
    if (arguments.size() > MaxArguments) return Fail(ERROR_FILENAME_EXCED_RANGE, L"启动参数过长（最多 8191 个字符）。");
    std::vector<Token> tokens;
    Result result = Tokens(arguments, tokens);
    if (!result) return result;
    unsigned counts[4]{};
    for (const auto& token : tokens) {
        if (token.value == L"--") break;
        const int kind = ManagedKind(token.value);
        if (kind && ++counts[kind] > 1)
            return Fail(ERROR_INVALID_PARAMETER, L"同一启动选项出现了两次，请检查额外参数与界面设置。");
        if (token.value == L"--launcher-check")
            return Fail(ERROR_INVALID_PARAMETER, L"--launcher-check 只用于诊断，不能保存为启动选项。");
    }
    return {};
}
Result ParseArguments(const std::wstring& arguments, Options& options) {
    options.proxy = false; options.silent = false; options.bypass.clear(); options.extra.clear();
    std::vector<Token> tokens;
    Result result = Tokens(arguments, tokens);
    if (!result) { options.extra = arguments; return result; }
    unsigned counts[4]{};
    for (const auto& token : tokens) {
        if (token.value == L"--") break;
        ++counts[ManagedKind(token.value)];
    }
    // Preserve the complete original text when duplicate managed options cannot be represented by the form.
    if (counts[1] > 1 || counts[2] > 1 || counts[3] > 1) { options.extra = arguments; return {}; }
    size_t lastUnknownEnd = 0;
    bool previousUnknown = false, positional = false;
    for (const auto& token : tokens) {
        if (token.value == L"--") positional = true;
        if (!positional && token.value == L"--no-startup-window") { options.silent = true; previousUnknown = false; continue; }
        if (!positional && token.value.rfind(L"--proxy-bypass-list=", 0) == 0 && token.value.size() > 20) {
            options.bypass = token.value.substr(20); previousUnknown = false; continue;
        }
        if (!positional && token.value.rfind(L"--proxy-server=", 0) == 0) {
            const std::wstring value = token.value.substr(15);
            const size_t separator = value.find(L"://"), colon = value.rfind(L':');
            if (separator != std::wstring::npos && colon > separator + 3) {
                Options candidate = options;
                candidate.proxy = true; candidate.protocol = value.substr(0, separator);
                candidate.host = value.substr(separator + 3, colon - separator - 3); candidate.port = value.substr(colon + 1);
                candidate.extra.clear();
                std::wstring ignored;
                if (BuildArguments(candidate, ignored)) {
                    options.proxy = true; options.protocol = candidate.protocol;
                    options.host = candidate.host; options.port = candidate.port;
                    previousUnknown = false;
                    continue;
                }
            }
        }
        if (previousUnknown) options.extra += arguments.substr(lastUnknownEnd, token.end - lastUnknownEnd);
        else Append(options.extra, token.raw);
        previousUnknown = true;
        lastUnknownEnd = token.end;
    }
    return {};
}
Result ReadLink(const std::wstring& path, Link& value) {
    Com<IShellLinkW> link;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link.p));
    if (FAILED(hr)) return Hr(hr, L"无法读取快捷方式。");
    Com<IPersistFile> persist;
    if (FAILED(hr = link->QueryInterface(IID_PPV_ARGS(&persist.p)))
        || FAILED(hr = persist->Load(path.c_str(), STGM_READ))) return Hr(hr, L"无法读取快捷方式：" + path);
    std::vector<wchar_t> target(32768), arguments(32768), directory(32768), description(1024);
    if (FAILED(hr = link->GetPath(target.data(), 32768, nullptr, SLGP_RAWPATH))
        || FAILED(hr = link->GetArguments(arguments.data(), 32768))
        || FAILED(hr = link->GetWorkingDirectory(directory.data(), 32768))
        || FAILED(hr = link->GetDescription(description.data(), 1024))) return Hr(hr, L"无法读取快捷方式属性。");
    value = { target.data(), arguments.data(), directory.data(), description.data() };
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
    if (loaded.hasSettings) {
        Result parsed = ParseArguments(saved.arguments, loaded.options);
        if (!parsed) loaded.notice = parsed.message;
        const DWORD a = GetFileAttributesW(loaded.managedTarget.c_str());
        loaded.deployed = a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
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
    Snapshot oldExe, oldSettings, oldStartup;
    result = ReadFileBytes(target, oldExe); if (!result) return result;
    result = ReadFileBytes(paths.settingsLink, oldSettings); if (!result) return result;
    result = ReadFileBytes(paths.startupLink, oldStartup); if (!result) return result;
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
    bool exeChanged = false, settingsChanged = false, startupChanged = false;
    auto rollback = [&](Result failure) {
        std::wstring restoreErrors;
        auto note = [&](const Result& restored) { if (!restored) restoreErrors += L"\n" + restored.message; };
        if (startupChanged) note(Restore(paths.startupLink, oldStartup));
        if (settingsChanged) note(Restore(paths.settingsLink, oldSettings));
        if (exeChanged) note(Restore(target, oldExe));
        failure.message += restoreErrors.empty() ? L"\n本次文件更改已撤销。" : L"\n部分更改未能恢复，请重新运行工具检查：" + restoreErrors;
        return failure;
    };
    if (!oldExe.exists || oldExe.data != bytes) {
        result = WriteAtomic(target, bytes); if (!result) return result;
        exeChanged = true;
    }
    Snapshot check;
    result = ReadFileBytes(target, check);
    if (!result) return rollback(result);
    if (!check.exists || check.data != bytes) return rollback(Fail(ERROR_CRC, L"部署后的程序校验失败。"));
    const Link value{ target, arguments, directory, Marker };
    result = WriteLink(paths.settingsLink, value); if (!result) return rollback(result);
    settingsChanged = true;
    if (options.startup) {
        result = WriteLink(paths.startupLink, value); if (!result) return rollback(result);
        startupChanged = true;
    } else if (oldStartup.exists) {
        if (!DeleteFileW(paths.startupLink.c_str())) return rollback(Fail(GetLastError(), L"无法关闭登录自启。"));
        startupChanged = true;
    }
    Loaded verify;
    result = Load(paths, verify);
    if (!result) return rollback(result);
    if (!verify.deployed || verify.options.startup != options.startup || !Same(verify.managedTarget, target))
        return rollback(Fail(ERROR_INVALID_DATA, L"保存后的配置校验失败。"));
    deployedTarget = target;
    return {};
}
} // namespace setup
