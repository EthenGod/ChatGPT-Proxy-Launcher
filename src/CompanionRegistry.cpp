// SPDX-License-Identifier: GPL-3.0-only
#include "CompanionManifest.h"
#include <shlobj.h>
#include <algorithm>

namespace chain {
using automation::Invalid;
namespace rules = automation::contract;
namespace {
void OwnedRecord(const Record& r) {
    try {
        r.json.Fields({L"schemaVersion", L"managedBy", L"id", L"manifestPath", L"overrides"});
        if (r.json.At(L"schemaVersion").Integer(0, MAXDWORD) != rules::RegistrySchema || r.json.At(L"managedBy").String() != rules::ManagedBy || r.json.At(L"id").String() != r.id)
            throw Invalid("foreign-registration");
    } catch (const Invalid&) { throw automation::IoError(ERROR_ALREADY_EXISTS); }
}
void AtomicRecord(const std::wstring& path, const std::string& bytes, const std::string* previous) {
    if (bytes.size() > rules::JsonBytes) throw Invalid("registration-too-large");
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
void Warn(Json& warnings, PCWSTR code, const std::wstring& message = L"") {
    auto warning = Json::Object(); warning[L"code"] = Json(code); warning[L"message"] = Json(message); warnings.array.push_back(warning);
}
Record ReadOwned(const std::wstring& path, const std::wstring& id, std::string& bytes) {
    Record r; r.path = path; r.id = id; bytes = automation::ReadBytes(path);
    r.json = automation::ParseJson(bytes); OwnedRecord(r); return r;
}
}
Json RegistryCommand(const std::wstring& operation, const setup::Paths& paths, const std::wstring& manifest,
    const std::wstring& inputId, const Json& request, bool replace, bool& committed, Json& warnings) {
    auto data = Json::Object(); committed = false;
    if (operation == L"companion-list") {
        const auto records = ReadRegistry(paths.defaultDirectory);
        data[L"entries"] = Json::Array(); for (const auto& r : records) data[L"entries"].array.push_back(Summary(r)); return data;
    }
    std::wstring id = inputId; Entry candidate; Json overrides = Json::Object();
    if (operation == L"companion-register") { candidate = ReadManifest(manifest); id = candidate.id; }
    else if (operation == L"companion-configure") {
        request.Fields({L"schemaVersion", L"id", L"overrides"});
        if (request.At(L"schemaVersion").Integer(0, MAXDWORD) != rules::SetupApi) throw Invalid("unsupported-schema-version");
        id = request.At(L"id").String(128); overrides = request.At(L"overrides");
        ValidateOverrides(overrides);
    }
    if (!ValidId(id)) throw Invalid("invalid-id");
    const auto path = Join(Join(paths.defaultDirectory, rules::RegistryFolder), id + L".json");
    const bool disabling = operation == L"companion-configure" && overrides.Has(L"enabled") && !overrides.At(L"enabled").Bool();
    const bool recovery = disabling || operation == L"companion-remove";
    std::vector<Record> records;
    try { records = ReadRegistry(paths.defaultDirectory); }
    catch (...) {
        if (!recovery) throw;
        // Recovery addresses exactly one owned record, without trusting the rest of the registry.
        Warn(warnings, L"registry-read-incomplete");
    }
    auto current = std::find_if(records.begin(), records.end(), [&](const Record& r) { return r.id == id; });
    bool exists = current != records.end();
    std::string previous;
    if (recovery) {
        try {
            auto owned = ReadOwned(path, id, previous);
            if (exists) *current = std::move(owned);
            else { records.push_back(std::move(owned)); current = records.end() - 1; exists = true; }
        } catch (const automation::IoError& ex) {
            if (ex.code != ERROR_FILE_NOT_FOUND && ex.code != ERROR_PATH_NOT_FOUND) throw;
            exists = false;
        }
    } else if (exists) {
        OwnedRecord(*current); previous = automation::ReadBytes(current->path);
        if (automation::WriteJson(automation::ParseJson(previous)) != automation::WriteJson(current->json)) throw Invalid("registration-changed");
    }
    data[L"id"] = Json(id);
    if (operation == L"companion-remove") {
        if (exists) { RemoveRecord(path, previous); committed = true; }
    } else {
        if (operation == L"companion-configure") {
            if (!exists) throw Invalid("not-registered");
            candidate = ReadRegisteredEntry(id, current->json.At(L"manifestPath").String(), overrides);
        } else if (exists) {
            if (!replace && !automation::SamePath(candidate.manifestPath, current->json.At(L"manifestPath").String())) throw automation::IoError(ERROR_ALREADY_EXISTS);
            overrides = current->json.At(L"overrides"); candidate = ReadRegisteredEntry(id, candidate.manifestPath, overrides);
        }
        if (candidate.id != id) throw Invalid("manifest-id-changed");
        if (!exists && records.size() >= rules::Entries) throw Invalid("entry-limit");
        if (candidate.enabled) CheckLegacy(paths, candidate);
        Record proposed; proposed.id = id; proposed.path = path; proposed.entry = candidate; proposed.json = Json::Object();
        proposed.json[L"schemaVersion"] = Json::Number(rules::RegistrySchema); proposed.json[L"managedBy"] = Json(rules::ManagedBy);
        proposed.json[L"id"] = Json(id); proposed.json[L"manifestPath"] = Json(candidate.manifestPath); proposed.json[L"overrides"] = overrides;
        if (exists) *current = proposed; else records.push_back(proposed);
        std::vector<size_t> order; if (candidate.enabled && !SortEntries(records, order)) throw Invalid("invalid-graph");
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
            Warn(warnings, L"dependency-unavailable", r.id);
        }
    }
    return data;
}
}
