// SPDX-License-Identifier: GPL-3.0-only
#include "CompanionManifest.h"
#include <shlobj.h>
#include <algorithm>

namespace chain {
using automation::Invalid;
namespace {
void OwnedRecord(const Record& r) {
    try {
        r.json.Fields({L"schemaVersion", L"managedBy", L"id", L"manifestPath", L"overrides"});
        if (r.json.At(L"schemaVersion").Integer(1, 1) != 1 || r.json.At(L"managedBy").String() != L"ChatGPTProxyLauncher" || r.json.At(L"id").String() != r.id)
            throw Invalid("foreign-registration");
    } catch (const Invalid&) { throw automation::IoError(ERROR_ALREADY_EXISTS); }
}
void AtomicRecord(const std::wstring& path, const std::string& bytes, const std::string* previous) {
    if (bytes.size() > 65536) throw Invalid("registration-too-large");
    automation::CheckPlainPath(path);
    const auto folder = automation::ParentPath(path);
    const int error = SHCreateDirectoryExW(nullptr, folder.c_str(), nullptr);
    if (error != ERROR_SUCCESS && error != ERROR_ALREADY_EXISTS && error != ERROR_FILE_EXISTS) throw automation::IoError(static_cast<DWORD>(error));
    automation::CheckPlainPath(path);
    GUID id{}; if (FAILED(CoCreateGuid(&id))) throw automation::IoError(ERROR_NOT_ENOUGH_MEMORY);
    wchar_t guid[40]{};
    if (!StringFromGUID2(id, guid, 40)) throw automation::IoError(ERROR_INVALID_DATA);
    const auto temp = path + L"." + guid + L".tmp";
    struct Temp { std::wstring path; bool owned = false; ~Temp() { if (owned) DeleteFileW(path.c_str()); } } cleanup{temp};
    {
        automation::Handle file(CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (!file) throw automation::IoError(GetLastError()); cleanup.owned = true; automation::WriteBytes(file.value, bytes);
    }
    if (previous && automation::ReadBytes(path) != *previous) throw automation::IoError(ERROR_RETRY);
    if (!MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_WRITE_THROUGH | (previous ? MOVEFILE_REPLACE_EXISTING : 0))) throw automation::IoError(GetLastError());
    cleanup.owned = false;
}
void RemoveRecord(const std::wstring& path, const std::string& previous) {
    automation::CheckPlainPath(path);
    automation::Handle file(CreateFileW(path.c_str(), GENERIC_READ | DELETE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    if (!file) throw automation::IoError(GetLastError());
    BY_HANDLE_FILE_INFORMATION info{};
    if (!GetFileInformationByHandle(file.value, &info)) throw automation::IoError(GetLastError());
    if (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT) || info.nFileSizeHigh || info.nFileSizeLow != previous.size()) throw Invalid("registration-changed");
    std::string check(previous.size(), '\0'); DWORD got = 0;
    if (!ReadFile(file.value, check.data(), static_cast<DWORD>(check.size()), &got, nullptr) || got != check.size() || check != previous) throw Invalid("registration-changed");
    FILE_DISPOSITION_INFO disposition{TRUE};
    if (!SetFileInformationByHandle(file.value, FileDispositionInfo, &disposition, sizeof(disposition))) throw automation::IoError(GetLastError());
}
void CheckLegacy(const setup::Paths& paths, const Entry& e) {
    const auto folder = Join(paths.defaultDirectory, companions::Folder);
    WIN32_FIND_DATAW d{}; HANDLE h = FindFirstFileW(Join(folder, L"*").c_str(), &d);
    if (h == INVALID_HANDLE_VALUE) return;
    struct Find { HANDLE h; ~Find() { FindClose(h); } } find{h};
    do {
        if (d.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) continue;
        const auto file = Join(folder, d.cFileName);
        if (automation::SamePath(file, e.target)) throw Invalid("legacy-entry-conflict");
        if (companions::FileKind(file.c_str()) == companions::Kind::Link) {
            setup::Link link;
            if (setup::ReadLink(file, link, true) && automation::SamePath(link.target, e.target)) throw Invalid("legacy-entry-conflict");
        }
    } while (FindNextFileW(h, &d));
}
Json Summary(const Record& r) {
    auto j = Json::Object(); j[L"id"] = Json(r.id); j[L"valid"] = Json(r.error.empty()); j[L"code"] = Json(r.error);
    if (r.error.empty()) { j[L"enabled"] = Json(r.entry.enabled); j[L"version"] = Json(r.entry.version); j[L"manifestPath"] = Json(r.entry.manifestPath); }
    return j;
}
}
Json RegistryCommand(const std::wstring& operation, const setup::Paths& paths, const std::wstring& manifest,
    const std::wstring& inputId, const Json& request, bool replace, bool& committed, Json& warnings) {
    auto records = ReadRegistry(paths.defaultDirectory);
    auto data = Json::Object(); committed = false;
    if (operation == L"companion-list") {
        data[L"entries"] = Json::Array(); for (const auto& r : records) data[L"entries"].array.push_back(Summary(r)); return data;
    }
    std::wstring id = inputId; Entry candidate; Json overrides = Json::Object();
    if (operation == L"companion-register") { candidate = ReadManifest(manifest); id = candidate.id; }
    else if (operation == L"companion-configure") {
        request.Fields({L"schemaVersion", L"id", L"overrides"});
        if (request.At(L"schemaVersion").Integer(0, MAXDWORD) != 1) throw Invalid("unsupported-schema-version");
        id = request.At(L"id").String(128); overrides = request.At(L"overrides");
    }
    if (!ValidId(id)) throw Invalid("invalid-id");
    auto current = std::find_if(records.begin(), records.end(), [&](const Record& r) { return r.id == id; });
    const bool exists = current != records.end();
    std::string previous;
    if (exists) {
        OwnedRecord(*current); previous = automation::ReadBytes(current->path);
        if (automation::WriteJson(automation::ParseJson(previous)) != automation::WriteJson(current->json)) throw Invalid("registration-changed");
    }
    const auto path = Join(Join(paths.defaultDirectory, L"CompanionRegistry"), id + L".json");
    data[L"id"] = Json(id);
    if (operation == L"companion-remove") {
        if (exists) { RemoveRecord(path, previous); committed = true; }
    } else {
        if (operation == L"companion-configure") {
            if (!exists) throw Invalid("not-registered");
            candidate = ReadManifest(current->json.At(L"manifestPath").String(), overrides);
        } else if (exists) {
            if (!replace && !automation::SamePath(candidate.manifestPath, current->json.At(L"manifestPath").String())) throw automation::IoError(ERROR_ALREADY_EXISTS);
            overrides = current->json.At(L"overrides"); candidate = ReadManifest(candidate.manifestPath, overrides);
        }
        if (candidate.id != id) throw Invalid("manifest-id-changed");
        if (!exists && records.size() >= 32) throw Invalid("entry-limit");
        if (candidate.enabled) CheckLegacy(paths, candidate);
        Record proposed; proposed.id = id; proposed.path = path; proposed.entry = candidate; proposed.json = Json::Object();
        proposed.json[L"schemaVersion"] = Json::Number(1); proposed.json[L"managedBy"] = Json(L"ChatGPTProxyLauncher");
        proposed.json[L"id"] = Json(id); proposed.json[L"manifestPath"] = Json(candidate.manifestPath); proposed.json[L"overrides"] = overrides;
        if (exists) *current = proposed; else records.push_back(proposed);
        std::vector<size_t> order; if (!SortEntries(records, order)) throw Invalid("invalid-graph");
        if (candidate.enabled) for (const auto& dep : candidate.dependencies) {
            const auto found = std::find_if(records.begin(), records.end(), [&](const Record& r) { return r.id == dep.id; });
            if (found == records.end() || !found->error.empty() || !found->entry.enabled
                || (!dep.minimumVersion.empty() && CompareVersion(found->entry.version, dep.minimumVersion) < 0)) throw Invalid("dependency-unavailable");
        }
        const auto bytes = automation::WriteJson(proposed.json);
        if (!exists || bytes != previous) { AtomicRecord(path, bytes, exists ? &previous : nullptr); committed = true; }
    }
    for (const auto& r : records) {
        if (!r.error.empty() || !r.entry.enabled || r.id == id) continue;
        for (const auto& dep : r.entry.dependencies) if (dep.id == id && (operation == L"companion-remove" || !candidate.enabled
            || (!dep.minimumVersion.empty() && CompareVersion(candidate.version, dep.minimumVersion) < 0))) {
            auto warning = Json::Object(); warning[L"code"] = Json(L"dependency-unavailable"); warning[L"message"] = Json(r.id); warnings.array.push_back(warning);
        }
    }
    return data;
}
}
