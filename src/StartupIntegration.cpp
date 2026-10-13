// SPDX-License-Identifier: GPL-3.0-only
#include "StartupIntegration.h"
#include <appmodel.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <xmllite.h>
#include <wrl/client.h>
#include <array>
#include <algorithm>

namespace setup {
namespace startup_detail {
constexpr wchar_t Family[] = L"OpenAI.Codex_2p2nqsd0c76g0";
constexpr wchar_t RunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t ApprovalKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";
struct Key {
    HKEY value = nullptr;
    ~Key() { if (value) RegCloseKey(value); }
};
bool SamePath(const std::wstring& a, const std::wstring& b) {
    return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_EQUAL;
}
bool Matches(const std::wstring& command, const std::vector<std::wstring>& executables) {
    if (command.empty() || command.size() > 32767 || command.find(L'\0') != std::wstring::npos) return false;
    int count = 0;
    PWSTR* args = CommandLineToArgvW(command.c_str(), &count);
    if (!args) return false;
    bool found = false;
    for (const auto& executable : executables) {
        if (count && SamePath(args[0], executable)) { found = true; break; }
    }
    LocalFree(args);
    return found;
}
bool ValidTaskId(const std::wstring& id) {
    if (id.empty() || id.size() > 128 || id == L"." || id == L"..") return false;
    for (wchar_t c : id) if (!((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z')
        || (c >= L'0' && c <= L'9') || c == L'_' || c == L'-' || c == L'.')) return false;
    return true;
}
HRESULT InspectStartupManifest(IStream* stream, bool& declared, std::vector<std::wstring>* taskIds = nullptr) {
    declared = false;
    if (taskIds) taskIds->clear();
    Microsoft::WRL::ComPtr<IXmlReader> reader;
    HRESULT hr = CreateXmlReader(__uuidof(IXmlReader), reinterpret_cast<void**>(reader.GetAddressOf()), nullptr);
    if (FAILED(hr)) return hr;
    if (FAILED(hr = reader->SetProperty(XmlReaderProperty_DtdProcessing, DtdProcessing_Prohibit))
        || FAILED(hr = reader->SetInput(stream))) return hr;
    auto attribute = [&](PCWSTR name) {
        std::wstring result;
        if (reader->MoveToAttributeByName(name, nullptr) == S_OK) {
            const wchar_t* value = nullptr;
            if (SUCCEEDED(reader->GetValue(&value, nullptr)) && value) result = value;
            reader->MoveToElement();
        }
        return result;
    };
    XmlNodeType type{};
    std::vector<std::wstring> found;
    bool inExtension = false;
    UINT extensionDepth = 0;
    std::wstring extensionNamespace;
    while ((hr = reader->Read(&type)) == S_OK) {
        UINT depth = 0;
        if (FAILED(hr = reader->GetDepth(&depth))) return hr;
        if (inExtension && depth <= extensionDepth) inExtension = false;
        if (type != XmlNodeType_Element) continue;
        const wchar_t* name = nullptr; const wchar_t* uri = nullptr;
        if (FAILED(hr = reader->GetLocalName(&name, nullptr)) || FAILED(hr = reader->GetNamespaceUri(&uri, nullptr))) return hr;
        if (!name || !uri) return E_UNEXPECTED;
        if (inExtension && depth == extensionDepth + 1 && !wcscmp(name, L"StartupTask") && extensionNamespace == uri) {
            const std::wstring id = attribute(L"TaskId");
            if (!ValidTaskId(id)) return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
            if (std::find(found.begin(), found.end(), id) == found.end()) found.push_back(id);
            continue;
        }
        if (wcscmp(name, L"Extension") || (wcscmp(uri, L"http://schemas.microsoft.com/appx/manifest/uap/windows10/5")
            && wcscmp(uri, L"http://schemas.microsoft.com/appx/manifest/desktop/windows10"))) continue;
        if (attribute(L"Category") != L"windows.startupTask") continue;
        std::wstring executable = attribute(L"Executable");
        for (wchar_t& ch : executable) if (ch == L'/') ch = L'\\';
        if (SamePath(executable, L"app\\ChatGPT.exe") && !reader->IsEmptyElement()) {
            inExtension = true; extensionDepth = depth;
            // Attribute navigation can invalidate the reader's string pointers.
            if (FAILED(hr = reader->GetNamespaceUri(&uri, nullptr)) || !uri) return E_UNEXPECTED;
            extensionNamespace = uri;
        }
    }
    if (FAILED(hr)) return hr;
    declared = !found.empty();
    if (taskIds) *taskIds = std::move(found);
    return S_OK;
}
DWORD InstalledExecutables(std::vector<std::wstring>& executables, bool& packagedTask, DWORD& manifestError,
    std::vector<std::wstring>* taskIds = nullptr) {
    UINT32 count = 0, size = 0;
    LONG result = GetPackagesByPackageFamily(Family, &count, nullptr, &size, nullptr);
    if (result == ERROR_SUCCESS && !count) return ERROR_NOT_FOUND;
    if (result != ERROR_INSUFFICIENT_BUFFER) return result;
    if (count > 1024 || size > 2097152) return ERROR_INVALID_DATA;
    std::vector<PWSTR> names(count);
    std::vector<wchar_t> strings(size);
    result = GetPackagesByPackageFamily(Family, &count, names.data(), &size, strings.data());
    if (result) return result;
    for (UINT32 i = 0; i < count; ++i) {
        // Public package APIs provide the registered identity and path; names alone never authorize a write.
        UINT32 identitySize = 0;
        result = PackageIdFromFullName(names[i], PACKAGE_INFORMATION_BASIC, &identitySize, nullptr);
        if (result != ERROR_INSUFFICIENT_BUFFER || identitySize < sizeof(PACKAGE_ID) || identitySize > 131072) continue;
        std::vector<BYTE> identity(identitySize);
        if (PackageIdFromFullName(names[i], PACKAGE_INFORMATION_BASIC, &identitySize, identity.data())) continue;
        const auto id = reinterpret_cast<const PACKAGE_ID*>(identity.data());
        if (!id->name || !id->publisherId || !id->resourceId || *id->resourceId
            || wcscmp(id->name, L"OpenAI.Codex") || wcscmp(id->publisherId, L"2p2nqsd0c76g0")) continue;
        std::vector<wchar_t> path(32768);
        UINT32 capacity = static_cast<UINT32>(path.size());
        if (GetPackagePathByFullName(names[i], &capacity, path.data())) continue;
        const std::wstring exe = Join(path.data(), L"app\\ChatGPT.exe");
        DWORD attributes = GetFileAttributesW(exe.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
            executables.push_back(exe);
            Microsoft::WRL::ComPtr<IStream> stream;
            HRESULT hr = SHCreateStreamOnFileEx(Join(path.data(), L"AppxManifest.xml").c_str(), STGM_READ | STGM_SHARE_DENY_NONE,
                0, FALSE, nullptr, stream.GetAddressOf());
            bool declared = false;
            std::vector<std::wstring> ids;
            if (SUCCEEDED(hr)) hr = InspectStartupManifest(stream.Get(), declared, &ids);
            if (FAILED(hr)) manifestError = static_cast<DWORD>(hr);
            else {
                packagedTask = packagedTask || declared;
                if (taskIds) for (const auto& task : ids)
                    if (std::find(taskIds->begin(), taskIds->end(), task) == taskIds->end()) taskIds->push_back(task);
            }
        }
    }
    return executables.empty() ? ERROR_NOT_FOUND : ERROR_SUCCESS;
}
DWORD ReadTaskState(HKEY key, DWORD& state) {
    DWORD type = 0, size = sizeof(state);
    const LSTATUS error = RegQueryValueExW(key, L"State", nullptr, &type, reinterpret_cast<BYTE*>(&state), &size);
    if (error) return static_cast<DWORD>(error);
    return type == REG_DWORD && size == sizeof(state) && state <= 4 ? ERROR_SUCCESS : ERROR_INVALID_DATA;
}
void DisablePackagedTasks(const std::vector<std::wstring>& taskIds, StartupReport& report) {
    report.packagedTasks = static_cast<unsigned>(taskIds.size());
    for (const auto& id : taskIds) {
        if (!ValidTaskId(id)) { report.packagedError = ERROR_INVALID_DATA; continue; }
        const std::wstring path = L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\SystemAppData\\"
            + std::wstring(Family) + L"\\" + id;
        Key key;
        LSTATUS error = RegOpenKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, KEY_QUERY_VALUE, &key.value);
        if (error) { report.packagedError = static_cast<DWORD>(error); continue; }
        DWORD previous = 0;
        DWORD status = ReadTaskState(key.value, previous);
        if (status) { report.packagedError = status; continue; }
        // MSIX startup State: 0 disabled, 1 user-disabled, 2 enabled, 3/4 policy-controlled.
        // Existing keys only; preserve all other values including UserEnabledStartupOnce.
        if (previous == 0 || previous == 1 || previous == 3) { ++report.packagedAlreadyOff; continue; }
        if (previous == 4) { report.packagedError = ERROR_ACCESS_DISABLED_BY_POLICY; continue; }
        Key writable;
        error = RegOpenKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &writable.value);
        if (error) { report.packagedError = static_cast<DWORD>(error); continue; }
        DWORD current = 0;
        status = ReadTaskState(writable.value, current);
        if (status || current != previous) { report.packagedError = status ? status : ERROR_RETRY; continue; }
        const DWORD disabled = 1;
        error = RegSetValueExW(writable.value, L"State", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&disabled), sizeof(disabled));
        if (error) { report.packagedError = static_cast<DWORD>(error); continue; }
        status = ReadTaskState(writable.value, current);
        if (!status && current == disabled) { ++report.packagedDisabled; continue; }
        report.packagedError = status ? status : ERROR_WRITE_FAULT;
        // Do not overwrite a concurrent user/policy change. If verification could not read
        // the value, attempt to restore the original DWORD and report any recovery failure.
        if (status) {
            error = RegSetValueExW(writable.value, L"State", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&previous), sizeof(previous));
            if (error) report.restoreError = static_cast<DWORD>(error);
            else {
                status = ReadTaskState(writable.value, current);
                if (status || current != previous) report.restoreError = status ? status : ERROR_WRITE_FAULT;
            }
        }
    }
}
StartupReport DisableRunEntries(HKEY root, PCWSTR runPath, PCWSTR approvalPath,
    const std::vector<std::wstring>& executables) {
    StartupReport report;
    if (executables.empty()) { report.error = ERROR_NOT_FOUND; return report; }
    Key run;
    LONG error = RegOpenKeyExW(root, runPath, 0, KEY_QUERY_VALUE, &run.value);
    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) return report;
    if (error) { report.error = static_cast<DWORD>(error); return report; }
    std::vector<wchar_t> name(16384), value(32768);
    for (DWORD index = 0; ; ++index) {
        DWORD nameSize = static_cast<DWORD>(name.size()), bytes = static_cast<DWORD>(value.size() * sizeof(wchar_t)), type = 0;
        error = RegEnumValueW(run.value, index, name.data(), &nameSize, nullptr, &type,
            reinterpret_cast<BYTE*>(value.data()), &bytes);
        if (error == ERROR_NO_MORE_ITEMS) break;
        if (error) { report.error = static_cast<DWORD>(error); break; }
        if ((type != REG_SZ && type != REG_EXPAND_SZ) || bytes < sizeof(wchar_t) || bytes % sizeof(wchar_t)) continue;
        const size_t length = bytes / sizeof(wchar_t);
        if (value[length - 1] != L'\0') continue;
        std::wstring command(value.data(), length - 1);
        if (type == REG_EXPAND_SZ) {
            std::vector<wchar_t> expanded(32768);
            const DWORD n = ExpandEnvironmentStringsW(command.c_str(), expanded.data(), static_cast<DWORD>(expanded.size()));
            if (!n || n > expanded.size()) continue;
            command = expanded.data();
        }
        if (!Matches(command, executables)) continue;
        ++report.matched;
        Key approval;
        error = RegCreateKeyExW(root, approvalPath, 0, nullptr, 0, KEY_QUERY_VALUE | KEY_SET_VALUE,
            nullptr, &approval.value, nullptr);
        if (error) { report.error = static_cast<DWORD>(error); continue; }
        // Same per-user disabled representation used by Electron's enabled:false implementation.
        // Preserve the original Run command so Windows Settings can enable it again.
        std::array<BYTE, 12> disabled{};
        disabled[0] = 3;
        error = RegSetValueExW(approval.value, name.data(), 0, REG_BINARY, disabled.data(), static_cast<DWORD>(disabled.size()));
        if (error) { report.error = static_cast<DWORD>(error); continue; }
        std::array<BYTE, 12> actual{};
        DWORD actualType = 0, actualSize = static_cast<DWORD>(actual.size());
        error = RegQueryValueExW(approval.value, name.data(), nullptr, &actualType, actual.data(), &actualSize);
        if (!error && actualType == REG_BINARY && actualSize == disabled.size() && actual == disabled) ++report.disabled;
        else report.error = error ? static_cast<DWORD>(error) : ERROR_INVALID_DATA;
    }
    return report;
}
} // namespace startup_detail
std::wstring StartupReport::Message() const {
    if (packagedTask) {
        std::wstring message;
        if (packagedTasks && packagedDisabled + packagedAlreadyOff == packagedTasks && !packagedError && !manifestError)
            message = packagedDisabled ? L"已将 ChatGPT 的商店应用自启设为关闭，并已回读确认。" : L"ChatGPT 的商店应用自启已经关闭，无需重复操作。";
        else message = L"已识别到 ChatGPT 的商店应用自启，但未能确认所有任务都已关闭，需要按上面的步骤手动核对。";
        if (packagedError) message += L"\n自启状态处理结果：" + ErrorText(packagedError);
        if (restoreError) message += L"\n恢复原状态未完成，请手动检查：" + ErrorText(restoreError);
        if (disabled) message += L"\n另外，已关闭识别到的传统自启项。";
        if (error) message += L"\n检查传统自启项时出现问题：" + ErrorText(error);
        return message;
    }
    if (disabled && disabled == matched && !error)
        return L"已将识别到的 ChatGPT 自启项标记为关闭。";
    if (disabled)
        return L"已关闭部分 ChatGPT 自启项，仍有项目需要手动核对。";
    if (error == ERROR_NOT_FOUND)
        return L"未找到可验证的 ChatGPT 安装，无法检查自带自启。此结果不影响已保存的启动器配置。";
    if (error)
        return L"未能自动处理 ChatGPT 的传统自启项：" + ErrorText(error)
            + L"\n这不影响已经保存的启动器配置。";
    if (manifestError)
        return L"未找到可处理的传统自启项，且无法读取应用自启声明。\n" + ErrorText(manifestError);
    return L"未找到可处理的传统自启项，这不代表自带自启仍然开启。如果你已经关闭，无需重复操作。";
}
StartupReport TryDisableOfficialStartup() {
    std::vector<std::wstring> executables;
    std::vector<std::wstring> taskIds;
    bool packagedTask = false; DWORD manifestError = 0;
    const DWORD error = startup_detail::InstalledExecutables(executables, packagedTask, manifestError, &taskIds);
    if (error) return { 0, 0, error };
    StartupReport report = startup_detail::DisableRunEntries(HKEY_CURRENT_USER, startup_detail::RunKey, startup_detail::ApprovalKey, executables);
    report.packagedTask = packagedTask; report.manifestError = manifestError;
    if (packagedTask && !manifestError) startup_detail::DisablePackagedTasks(taskIds, report);
    return report;
}
Result OpenStartupSettings(HWND owner) {
    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info); info.hwnd = owner; info.fMask = SEE_MASK_FLAG_NO_UI;
    info.lpVerb = L"open"; info.lpFile = L"ms-settings:startupapps"; info.nShow = SW_SHOWNORMAL;
    if (ShellExecuteExW(&info)) return {};
    const DWORD error = GetLastError();
    return { error ? error : ERROR_GEN_FAILURE, L"未能打开 Windows 设置，请手动进入「设置 → 应用 → 启动」检查。" };
}
} // namespace setup
