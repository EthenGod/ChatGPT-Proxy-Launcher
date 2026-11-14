// SPDX-License-Identifier: GPL-3.0-only
#include "CompanionManifest.h"
#include <algorithm>
#include <functional>
#include <set>
#include <array>

namespace chain {
using automation::Invalid;
namespace rules = automation::contract;
std::wstring Join(const std::wstring& directory, const std::wstring& leaf) { if (directory.empty()) throw Invalid("empty-directory"); return directory + (directory.back() == L'\\' ? L"" : L"\\") + leaf; }
bool ValidId(const std::wstring& id) {
    if (id.empty() || id.size() > 128 || id[0] < L'a' || id[0] > L'z' || id.find(L'.') == id.npos) return false;
    bool separator = false;
    for (const auto c : id) {
        const bool sep = c == L'.' || c == L'-';
        if (sep && separator) return false;
        if (!sep && !(c >= L'a' && c <= L'z') && !(c >= L'0' && c <= L'9')) return false;
        separator = sep;
    }
    if (separator) return false;
    try { automation::FullPath(L"C:\\" + id + L".json"); } catch (...) { return false; }
    return true;
}
namespace {
std::array<unsigned long, 3> Version(const std::wstring& value) {
    if (value.size() > 32) throw Invalid("invalid-version");
    std::array<unsigned long, 3> fields{}; size_t begin = 0;
    for (size_t i = 0; i < 3; ++i) {
        const auto end = value.find(L'.', begin); const auto part = value.substr(begin, end == value.npos ? end : end - begin);
        if (part.empty() || part.size() > 10 || part.find_first_not_of(L"0123456789") != part.npos || (part.size() > 1 && part[0] == L'0')
            || (i < 2 && end == value.npos) || (i == 2 && end != value.npos)) throw Invalid("invalid-version");
        const auto n = _wcstoui64(part.c_str(), nullptr, 10); if (n > 2147483647) throw Invalid("version-overflow");
        fields[i] = static_cast<unsigned long>(n); begin = end + 1;
    }
    return fields;
}
std::vector<std::wstring> Arguments(const Json& value) {
    if (value.type != Json::Type::Array || value.array.size() > rules::Arguments) throw Invalid("invalid-arguments");
    std::vector<std::wstring> args;
    for (const auto& arg : value.array) args.push_back(arg.String(rules::ArgumentChars));
    return args;
}
std::wstring Relative(const std::wstring& base, const std::wstring& rel, bool directory) {
    if (rel.empty() || rel.size() > 1024 || rel[0] == L'\\' || rel.find(L':') != rel.npos || rel.find(L'/') != rel.npos) throw Invalid("invalid-relative-path");
    for (size_t pos = 0; pos < rel.size();) {
        const auto next = rel.find(L'\\', pos);
        if (rel.substr(pos, next == rel.npos ? next : next - pos) == L"..") throw Invalid("path-escapes-package");
        if (next == rel.npos) break; pos = next + 1;
    }
    const auto full = automation::FullPath(Join(base, rel));
    const auto prefix = base + (base.back() == L'\\' ? L"" : L"\\");
    if (!automation::SamePath(full, base) && (full.size() < prefix.size() || !automation::SamePath(full.substr(0, prefix.size()), prefix))) throw Invalid("path-escapes-package");
    automation::CheckPlainPath(full);
    const DWORD a = GetFileAttributesW(full.c_str());
    if (a == INVALID_FILE_ATTRIBUTES || ((a & FILE_ATTRIBUTE_DIRECTORY) != 0) != directory) throw Invalid(directory ? "working-directory-missing" : "entry-missing");
    return full;
}
void ApplyOverrides(Entry& e, const Json& overrides) {
    overrides.Fields({L"enabled", L"arguments", L"config", L"timeoutMs", L"maxAttempts"});
    if (overrides.Has(L"enabled")) e.enabled = overrides.At(L"enabled").Bool();
    if (overrides.Has(L"arguments")) e.arguments = Arguments(overrides.At(L"arguments"));
    if (overrides.Has(L"config")) e.config = overrides.At(L"config");
    if (overrides.Has(L"timeoutMs")) e.timeout = static_cast<DWORD>(overrides.At(L"timeoutMs").Integer(rules::TimeoutMinMs, rules::TimeoutMaxMs));
    if (overrides.Has(L"maxAttempts")) e.attempts = static_cast<DWORD>(overrides.At(L"maxAttempts").Integer(1, rules::AttemptsMax));
    if (e.config.type != Json::Type::Object) throw Invalid("config-must-be-object");
}
}
int CompareVersion(const std::wstring& left, const std::wstring& right) { const auto a = Version(left), b = Version(right); return a == b ? 0 : a < b ? -1 : 1; }
void ValidateOverrides(const Json& overrides) { Entry value; ApplyOverrides(value, overrides); }
Entry ReadRegisteredEntry(const std::wstring& id, const std::wstring& manifest, const Json& overrides) {
    if (overrides.Has(L"enabled") && !overrides.At(L"enabled").Bool()) {
        Entry e; e.id = id; e.manifestPath = automation::FullPath(manifest); ApplyOverrides(e, overrides); return e;
    }
    auto entry = ReadManifest(manifest, overrides);
    if (entry.id != id) throw Invalid("manifest-id-changed");
    return entry;
}
Entry ReadManifest(const std::wstring& file, const Json& overrides) {
    Entry e; e.manifestPath = automation::FullPath(file);
    const auto j = automation::ParseJson(automation::ReadBytes(e.manifestPath));
    j.Fields({L"schemaVersion", L"id", L"name", L"version", L"requires", L"entry", L"dependsOn", L"readiness", L"retry", L"config"});
    if (j.At(L"schemaVersion").Integer(0, MAXDWORD) != rules::ManifestSchema) throw Invalid("unsupported-schema-version");
    e.id = j.At(L"id").String(128); if (!ValidId(e.id)) throw Invalid("invalid-id");
    e.name = j.At(L"name").String(80); if (e.name.empty()) throw Invalid("empty-name");
    e.version = j.At(L"version").String(32); Version(e.version);
    const auto& requirements = j.At(L"requires");
    if (requirements.type != Json::Type::Array || requirements.array.size() > rules::Capabilities) throw Invalid("invalid-capabilities");
    std::set<std::wstring> capabilities;
    for (const auto& item : requirements.array) {
        const auto value = item.String(64);
        if (!rules::SupportsManifestCapability(value)) throw Invalid("unsupported-capability");
        if (!capabilities.insert(value).second) throw Invalid("duplicate-capability");
    }
    const auto& entry = j.At(L"entry"); entry.Fields({L"runtime", L"path", L"workingDirectory", L"arguments", L"hostPath", L"window"});
    const auto runtime = entry.At(L"runtime").String(16);
    if (runtime == L"exe") e.kind = companions::Kind::Exe;
    else if (runtime == L"powershell") e.kind = companions::Kind::PowerShell;
    else if (runtime == L"wscript") e.kind = companions::Kind::Wsh;
    else if (runtime == L"python") e.kind = companions::Kind::Python;
    else if (runtime == L"pythonw") e.kind = companions::Kind::PythonWindow;
    else if (runtime == L"node") e.kind = companions::Kind::Node;
    else if (runtime == L"java") e.kind = companions::Kind::Jar;
    else if (runtime == L"batch") e.kind = companions::Kind::Batch;
    else throw Invalid("unknown-runtime");
    const auto base = automation::ParentPath(e.manifestPath);
    e.target = Relative(base, entry.At(L"path").String(1024), false);
    e.directory = Relative(base, entry.Has(L"workingDirectory") ? entry.At(L"workingDirectory").String(1024) : L".", true);
    auto kind = companions::FileKind(e.target.c_str());
    const auto dot = e.target.find_last_of(L'.');
    if (dot == e.target.npos) throw Invalid("runtime-file-mismatch");
    const auto ext = e.target.substr(dot);
    if (e.kind == companions::Kind::Node && !_wcsicmp(ext.c_str(), L".js")) kind = companions::Kind::Node;
    if (kind != e.kind || !companions::ValidTarget(e.target.c_str(), L"", e.directory.c_str())) throw Invalid("runtime-file-mismatch");
    if (entry.Has(L"arguments")) e.arguments = Arguments(entry.At(L"arguments"));
    if (entry.Has(L"window")) {
        const auto& window = entry.At(L"window").String();
        if (window != L"normal" && window != L"hidden") throw Invalid("invalid-window-mode");
        e.hidden = window == L"hidden";
    }
    if (entry.Has(L"hostPath")) {
        if (e.kind == companions::Kind::Exe || e.kind == companions::Kind::Batch) throw Invalid("unexpected-host-path");
        e.host = automation::FullPath(entry.At(L"hostPath").String()); automation::CheckPlainPath(e.host);
        if (companions::FileKind(e.host.c_str()) != companions::Kind::Exe || !companions::HostFile(e.host)) throw Invalid("runtime-missing");
    }
    if (j.Has(L"dependsOn")) {
        const auto& deps = j.At(L"dependsOn"); if (deps.type != Json::Type::Array || deps.array.size() >= rules::Entries) throw Invalid("invalid-dependencies");
        std::set<std::wstring> ids;
        for (const auto& dep : deps.array) {
            dep.Fields({L"id", L"minimumVersion"}); Dependency d; d.id = dep.At(L"id").String(128);
            if (!ValidId(d.id) || d.id == e.id || !ids.insert(d.id).second) throw Invalid("invalid-dependency-id");
            if (dep.Has(L"minimumVersion")) { d.minimumVersion = dep.At(L"minimumVersion").String(32); Version(d.minimumVersion); }
            e.dependencies.push_back(d);
        }
    }
    const auto& readiness = j.At(L"readiness"); const auto mode = readiness.At(L"mode").String();
    e.signal = mode == L"signal"; if (!e.signal && mode != L"exit-code") throw Invalid("invalid-readiness");
    if (e.signal) { readiness.Fields({L"mode", L"timeoutMs"}); if (!capabilities.count(rules::ReadyCapability)) throw Invalid("missing-ready-capability"); }
    else readiness.Fields({L"mode", L"timeoutMs", L"successExitCodes"});
    if (readiness.Has(L"timeoutMs")) e.timeout = static_cast<DWORD>(readiness.At(L"timeoutMs").Integer(rules::TimeoutMinMs, rules::TimeoutMaxMs));
    if (readiness.Has(L"successExitCodes")) {
        const auto& codes = readiness.At(L"successExitCodes");
        if (codes.type != Json::Type::Array || codes.array.empty() || codes.array.size() > 16) throw Invalid("invalid-exit-codes");
        e.exitCodes.clear(); std::set<DWORD> unique;
        for (const auto& code : codes.array) { const auto n = static_cast<DWORD>(code.Integer(0, MAXDWORD)); if (!unique.insert(n).second) throw Invalid("duplicate-exit-code"); e.exitCodes.push_back(n); }
    }
    if (j.Has(L"retry")) {
        const auto& retry = j.At(L"retry"); retry.Fields({L"safe", L"maxAttempts", L"delayMs"});
        if (retry.Has(L"safe")) e.safe = retry.At(L"safe").Bool();
        if (retry.Has(L"maxAttempts")) e.attempts = static_cast<DWORD>(retry.At(L"maxAttempts").Integer(1, rules::AttemptsMax));
        if (retry.Has(L"delayMs")) e.delay = static_cast<DWORD>(retry.At(L"delayMs").Integer(rules::RetryMinMs, rules::RetryMaxMs));
    }
    if (j.Has(L"config")) e.config = j.At(L"config");
    ApplyOverrides(e, overrides);
    if ((e.attempts > 1 && !e.safe) || (e.safe && !capabilities.count(rules::RetryCapability))) throw Invalid("unsafe-retry");
    return e;
}
std::vector<Record> ReadRegistry(const std::wstring& root) {
    const auto folder = Join(root, rules::RegistryFolder); automation::CheckPlainPath(folder);
    std::vector<Record> records;
    WIN32_FIND_DATAW data{}; HANDLE raw = FindFirstFileW(Join(folder, L"*.json").c_str(), &data);
    if (raw == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND) return records;
        throw automation::IoError(GetLastError());
    }
    struct Find { HANDLE value; ~Find() { FindClose(value); } } find{raw};
    do {
        if (records.size() == rules::Entries) throw Invalid("entry-limit");
        Record r; r.path = Join(folder, data.cFileName); r.id = data.cFileName; r.id.resize(r.id.size() - 5);
        try {
            if (!ValidId(r.id) || (data.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) throw Invalid("invalid-registration-file");
            r.json = automation::ParseJson(automation::ReadBytes(r.path));
            r.json.Fields({L"schemaVersion", L"managedBy", L"id", L"manifestPath", L"overrides"});
            if (r.json.At(L"schemaVersion").Integer(0, MAXDWORD) != rules::RegistrySchema) throw Invalid("unsupported-schema-version");
            if (r.json.At(L"managedBy").String() != rules::ManagedBy || r.json.At(L"id").String() != r.id) throw Invalid("foreign-registration");
            r.entry = ReadRegisteredEntry(r.id, r.json.At(L"manifestPath").String(), r.json.At(L"overrides"));
        } catch (const Invalid& ex) { r.error = automation::FromUtf8(ex.what()); }
        catch (const automation::IoError&) { r.error = L"read-failed"; }
        records.push_back(std::move(r));
    } while (FindNextFileW(raw, &data));
    if (GetLastError() != ERROR_NO_MORE_FILES) throw automation::IoError(GetLastError());
    std::sort(records.begin(), records.end(), [](const Record& a, const Record& b) { return a.id < b.id; }); return records;
}
bool SortEntries(const std::vector<Record>& records, std::vector<size_t>& order) {
    order.clear(); std::vector<int> state(records.size());
    std::function<bool(size_t)> visit = [&](size_t i) {
        if (state[i] == 1) return false;
        if (state[i] == 2) return true;
        state[i] = 1;
        if (records[i].error.empty() && records[i].entry.enabled) for (const auto& dep : records[i].entry.dependencies) {
            const auto it = std::find_if(records.begin(), records.end(), [&](const Record& r) { return r.id == dep.id; });
            if (it != records.end() && !visit(static_cast<size_t>(it - records.begin()))) return false;
        }
        state[i] = 2; order.push_back(i); return true;
    };
    for (size_t i = 0; i < records.size(); ++i) if (!visit(i)) return false;
    return true;
}
}
