// SPDX-License-Identifier: GPL-3.0-only
#include "DeploymentDiscovery.h"
#include "SetupCore.h"
#include <shlobj.h>
#include <set>

namespace automation {
namespace {
template<class T> struct ComPtr { T* p = nullptr; ~ComPtr() { if (p) p->Release(); } };
bool Missing(DWORD error) { return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND; }
std::wstring Join(const std::wstring& root, const std::wstring& leaf) {
    if (root.empty()) throw Invalid("empty-directory");
    return root + (root.back() == L'\\' ? L"" : L"\\") + leaf;
}
void Versions(const Json& list) {
    if (list.type != Json::Type::Array || list.array.size() > contract::Capabilities) throw Invalid("invalid-api-versions");
    std::set<long long> seen;
    for (const auto& value : list.array) if (!seen.insert(value.Integer(1, MAXDWORD)).second) throw Invalid("duplicate-api-version");
}
void ValidateDiscovery(const Json& record, const std::wstring& path, const std::wstring& version) {
    record.Fields({L"schemaVersion", L"managedBy", L"launcherPath", L"launcherVersion", L"setupApiVersions", L"chainProtocolVersions", L"capabilities"});
    if (record.At(L"schemaVersion").Integer(0, MAXDWORD) != contract::DiscoverySchema
        || record.At(L"managedBy").String() != contract::ManagedBy || version.empty()
        || !SamePath(FullPath(record.At(L"launcherPath").String()), path)
        || record.At(L"launcherVersion").String() != version) throw Invalid("invalid-discovery");
    Versions(record.At(L"setupApiVersions")); Versions(record.At(L"chainProtocolVersions"));
    const auto& list = record.At(L"capabilities");
    if (list.type != Json::Type::Array || list.array.size() > contract::Capabilities) throw Invalid("invalid-capabilities");
    std::set<std::wstring> seen;
    for (const auto& value : list.array) {
        const auto& name = value.String(64);
        if (name.empty() || name[0] < L'a' || name[0] > L'z' || name.find_first_not_of(L"abcdefghijklmnopqrstuvwxyz0123456789-") != name.npos
            || !seen.insert(name).second) throw Invalid("invalid-capability");
    }
}
}
std::wstring ReadAppVersion(const std::wstring& path) {
    DWORD ignored = 0;
    const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &ignored);
    if (!size || size > 1024 * 1024) return {};
    std::vector<BYTE> data(size);
    if (!GetFileVersionInfoW(path.c_str(), 0, size, data.data())) return {};
    LPVOID value = nullptr; UINT length = 0;
    if (!VerQueryValueW(data.data(), L"\\", &value, &length) || !value || length < sizeof(VS_FIXEDFILEINFO)) return {};
    VS_FIXEDFILEINFO info{}; CopyMemory(&info, value, sizeof(info));
    if (info.dwSignature != VS_FFI_SIGNATURE || info.dwFileType != VFT_APP || (!info.dwFileVersionMS && !info.dwFileVersionLS)) return {};
    auto version = std::to_wstring(HIWORD(info.dwFileVersionMS)) + L"." + std::to_wstring(LOWORD(info.dwFileVersionMS))
        + L"." + std::to_wstring(HIWORD(info.dwFileVersionLS));
    if (LOWORD(info.dwFileVersionLS)) version += L"." + std::to_wstring(LOWORD(info.dwFileVersionLS));
    return version;
}
Json CapabilityList(bool launcherOnly) {
    auto list = Json::Array();
    for (const auto& cap : contract::CapabilityTable) if (!launcherOnly || cap.launcher) list.array.emplace_back(cap.name);
    return list;
}
Json MakeDiscoveryRecord(const std::wstring& path, const std::wstring& version) {
    auto record = Json::Object();
    record[L"schemaVersion"] = Json::Number(contract::DiscoverySchema);
    record[L"managedBy"] = Json(contract::ManagedBy);
    record[L"launcherPath"] = Json(path); record[L"launcherVersion"] = Json(version);
    record[L"setupApiVersions"] = Json::Array(); record[L"setupApiVersions"].array.push_back(Json::Number(contract::SetupApi));
    record[L"chainProtocolVersions"] = Json::Array(); record[L"chainProtocolVersions"].array.push_back(Json::Number(contract::ChainProtocol));
    record[L"capabilities"] = CapabilityList(); return record;
}
Json ReadDeploymentInfo(const std::wstring& root, const std::wstring& settings) {
    auto result = Json::Object(); result[L"schemaVersion"] = Json::Number(contract::DiscoverySchema);
    result[L"state"] = Json(L"not-deployed"); result[L"launcherPath"] = Json(L""); result[L"launcherVersion"] = Json(L"");
    try {
        CheckPlainPath(settings);
        if (GetFileAttributesW(settings.c_str()) == INVALID_FILE_ATTRIBUTES) {
            const DWORD error = GetLastError(); if (!Missing(error)) throw IoError(error); return result;
        }
        const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        struct Apartment { HRESULT hr; ~Apartment() { if (SUCCEEDED(hr)) CoUninitialize(); } } apartment{com};
        if (FAILED(com) && com != RPC_E_CHANGED_MODE) throw IoError(ERROR_NOT_SUPPORTED);
        ComPtr<IShellLinkW> link; ComPtr<IPersistFile> persist;
        if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link.p)))
            || FAILED(link.p->QueryInterface(IID_PPV_ARGS(&persist.p)))) throw IoError(ERROR_NOT_SUPPORTED);
        const HRESULT loaded = persist.p->Load(settings.c_str(), STGM_READ);
        if (loaded == E_ACCESSDENIED || loaded == STG_E_ACCESSDENIED || loaded == STG_E_SHAREVIOLATION
            || loaded == STG_E_LOCKVIOLATION || loaded == HRESULT_FROM_WIN32(ERROR_SHARING_VIOLATION)) throw IoError(static_cast<DWORD>(loaded));
        if (FAILED(loaded)) throw Invalid("invalid-settings");
        std::vector<wchar_t> path(32768), marker(256);
        if (FAILED(link.p->GetPath(path.data(), static_cast<int>(path.size()), nullptr, SLGP_RAWPATH))
            || FAILED(link.p->GetDescription(marker.data(), static_cast<int>(marker.size()))) || wcscmp(marker.data(), setup::Marker)) throw Invalid("foreign-settings");
        const auto target = FullPath(path.data()); CheckPlainPath(target);
        if (!SamePath(Join(ParentPath(target), setup::LauncherName), target)) throw Invalid("invalid-target");
        result[L"launcherPath"] = Json(target);
        const DWORD attr = GetFileAttributesW(target.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES) {
            const DWORD error = GetLastError(); if (!Missing(error)) throw IoError(error); result[L"state"] = Json(L"missing"); return result;
        }
        if (attr & FILE_ATTRIBUTE_DIRECTORY) throw Invalid("invalid-target");
        const auto version = ReadAppVersion(target); result[L"launcherVersion"] = Json(version); result[L"state"] = Json(L"legacy");
        const auto discovery = Join(root, contract::DiscoveryFile);
        if (GetFileAttributesW(discovery.c_str()) == INVALID_FILE_ATTRIBUTES) {
            const DWORD error = GetLastError(); if (!Missing(error)) throw IoError(error); return result;
        }
        auto record = ParseJson(ReadBytes(discovery)); ValidateDiscovery(record, target, version);
        result[L"discovery"] = record; result[L"state"] = Json(L"managed");
    } catch (const IoError&) { result[L"state"] = Json(L"unknown"); }
    catch (const Invalid&) { result[L"state"] = Json(L"invalid"); }
    return result;
}
}
