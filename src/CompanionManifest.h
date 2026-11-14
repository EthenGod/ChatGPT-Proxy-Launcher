// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "AutomationIO.h"
#include "SetupCore.h"
#include "StartupApps.h"

namespace chain {
using automation::Json;
struct Dependency { std::wstring id, minimumVersion; };
struct Entry {
    std::wstring id, name, version, manifestPath, target, directory, host;
    companions::Kind kind = companions::Kind::Unknown;
    std::vector<std::wstring> arguments;
    std::vector<Dependency> dependencies;
    Json config = Json::Object();
    bool enabled = true, signal = true, safe = false, hidden = false;
    DWORD timeout = automation::contract::TimeoutDefaultMs, attempts = 1, delay = automation::contract::RetryDefaultMs;
    std::vector<DWORD> exitCodes{0};
};
struct Record { std::wstring id, path, error; Entry entry; Json json; };
std::wstring Join(const std::wstring& directory, const std::wstring& leaf);
bool ValidId(const std::wstring& id);
int CompareVersion(const std::wstring& left, const std::wstring& right);
Entry ReadManifest(const std::wstring& file, const Json& overrides = Json::Object());
void ValidateOverrides(const Json& overrides);
// Disabled registrations remain manageable even when their package is unavailable.
Entry ReadRegisteredEntry(const std::wstring& id, const std::wstring& manifest, const Json& overrides);
std::vector<Record> ReadRegistry(const std::wstring& root);
bool SortEntries(const std::vector<Record>& records, std::vector<size_t>& order);
Json RegistryCommand(const std::wstring& operation, const setup::Paths& paths, const std::wstring& manifest,
    const std::wstring& id, const Json& request, bool replace, bool& committed, Json& warnings);
}
