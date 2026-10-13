// SPDX-License-Identifier: GPL-3.0-only
#include "../src/StartupIntegration.h"
#include <map>
#include <stdio.h>

struct Entry { std::wstring name, command; DWORD type = REG_SZ; };
static std::vector<Entry> entries;
static std::map<std::wstring, std::vector<BYTE>> approvalFixtures;
static DWORD failWrite = 0, failOpen = 0;
static unsigned writes = 0;
static const HKEY FakeRun = reinterpret_cast<HKEY>(static_cast<UINT_PTR>(11));
static const HKEY FakeApproval = reinterpret_cast<HKEY>(static_cast<UINT_PTR>(12));
static const HKEY FakeTask = reinterpret_cast<HKEY>(static_cast<UINT_PTR>(13));
static const wchar_t TaskPath[] = L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\SystemAppData\\OpenAI.Codex_2p2nqsd0c76g0\\App";
static DWORD taskState = 2, taskType = REG_DWORD, failTaskOpen = 0;
static unsigned taskWrites = 0, taskQueries = 0, failTaskQueryAt = 0, failTaskWriteAt = 0;
static bool taskRace = false;
static LSTATUS WINAPI FakeOpen(HKEY root, LPCWSTR path, DWORD, REGSAM access, PHKEY out) {
    if (!wcscmp(path, TaskPath)) {
        if (root != HKEY_CURRENT_USER || (access != KEY_QUERY_VALUE && access != (KEY_QUERY_VALUE | KEY_SET_VALUE))) return ERROR_ACCESS_DENIED;
        if (failTaskOpen) return static_cast<LSTATUS>(failTaskOpen);
        *out = FakeTask; return ERROR_SUCCESS;
    }
    if (wcscmp(path, L"mock")) return ERROR_FILE_NOT_FOUND;
    if (failOpen) return static_cast<LSTATUS>(failOpen);
    if (access != KEY_QUERY_VALUE) return ERROR_ACCESS_DENIED;
    *out = FakeRun; return ERROR_SUCCESS;
}
static LSTATUS WINAPI FakeCreate(HKEY, LPCWSTR, DWORD, LPWSTR, DWORD, REGSAM, LPSECURITY_ATTRIBUTES, PHKEY out, LPDWORD) {
    *out = FakeApproval; return ERROR_SUCCESS;
}
static LSTATUS WINAPI FakeEnum(HKEY key, DWORD index, LPWSTR name, LPDWORD length, LPDWORD, LPDWORD type, LPBYTE bytes, LPDWORD size) {
    if (key != FakeRun) return ERROR_INVALID_HANDLE;
    if (index >= entries.size()) return ERROR_NO_MORE_ITEMS;
    const Entry& entry = entries[index];
    const DWORD required = static_cast<DWORD>((entry.command.size() + 1) * sizeof(wchar_t));
    if (*length <= entry.name.size() || *size < required) return ERROR_MORE_DATA;
    CopyMemory(name, entry.name.c_str(), (entry.name.size() + 1) * sizeof(wchar_t));
    CopyMemory(bytes, entry.command.c_str(), required);
    *length = static_cast<DWORD>(entry.name.size()); *size = required; *type = entry.type;
    return ERROR_SUCCESS;
}
static LSTATUS WINAPI FakeSet(HKEY key, LPCWSTR name, DWORD, DWORD type, const BYTE* bytes, DWORD size) {
    if (key == FakeTask) {
        if (wcscmp(name, L"State") || type != REG_DWORD || size != sizeof(DWORD)) return ERROR_INVALID_PARAMETER;
        if (++taskWrites == failTaskWriteAt) return ERROR_ACCESS_DENIED;
        CopyMemory(&taskState, bytes, size); return ERROR_SUCCESS;
    }
    if (key != FakeApproval || type != REG_BINARY || size != 12) return ERROR_INVALID_PARAMETER;
    ++writes;
    if (failWrite) return static_cast<LSTATUS>(failWrite);
    approvalFixtures[name] = std::vector<BYTE>(bytes, bytes + size); return ERROR_SUCCESS;
}
static LSTATUS WINAPI FakeQuery(HKEY key, LPCWSTR name, LPDWORD, LPDWORD type, LPBYTE bytes, LPDWORD size) {
    if (key == FakeTask) {
        if (wcscmp(name, L"State") || *size < sizeof(DWORD)) return ERROR_INVALID_PARAMETER;
        if (++taskQueries == failTaskQueryAt) return ERROR_ACCESS_DENIED;
        if (taskRace && taskQueries == 2) taskState = 3;
        *type = taskType; *size = sizeof(DWORD); CopyMemory(bytes, &taskState, sizeof(DWORD)); return ERROR_SUCCESS;
    }
    const auto found = approvalFixtures.find(name);
    if (found == approvalFixtures.end()) return ERROR_FILE_NOT_FOUND;
    if (*size < found->second.size()) return ERROR_MORE_DATA;
    CopyMemory(bytes, found->second.data(), found->second.size());
    *size = static_cast<DWORD>(found->second.size()); *type = REG_BINARY; return ERROR_SUCCESS;
}
static LSTATUS WINAPI FakeClose(HKEY) { return ERROR_SUCCESS; }
#define RegOpenKeyExW FakeOpen
#define RegCreateKeyExW FakeCreate
#define RegEnumValueW FakeEnum
#define RegSetValueExW FakeSet
#define RegQueryValueExW FakeQuery
#define RegCloseKey FakeClose
#include "../src/StartupIntegration.cpp"
#undef RegOpenKeyExW
#undef RegCreateKeyExW
#undef RegEnumValueW
#undef RegSetValueExW
#undef RegQueryValueExW
#undef RegCloseKey
#define REQUIRE(condition) do { if (!(condition)) { printf("FAIL line %d: %s\n", __LINE__, #condition); return 20; } } while (0)
static HRESULT InspectXml(const char* xml, bool& declared, std::vector<std::wstring>* ids = nullptr) {
    IStream* stream = SHCreateMemStream(reinterpret_cast<const BYTE*>(xml), static_cast<UINT>(strlen(xml)));
    if (!stream) return E_OUTOFMEMORY;
    const HRESULT hr = setup::startup_detail::InspectStartupManifest(stream, declared, ids);
    stream->Release(); return hr;
}
int main() {
    using namespace setup::startup_detail;
    bool declared = false;
    std::vector<std::wstring> taskIds;
    REQUIRE(SUCCEEDED(InspectXml("<Package xmlns:u='http://schemas.microsoft.com/appx/manifest/uap/windows10/5'>"
        "<u:Extension Category='windows.startupTask' Executable='app/ChatGPT.exe'>"
        "<u:StartupTask TaskId='App' Enabled='true'/></u:Extension></Package>", declared, &taskIds)) && declared);
    REQUIRE(taskIds == std::vector<std::wstring>{L"App"});
    REQUIRE(SUCCEEDED(InspectXml("<Package xmlns:d='http://schemas.microsoft.com/appx/manifest/desktop/windows10'>"
        "<d:Extension Category='windows.startupTask' Executable='app\\ChatGPT.exe'>"
        "<d:StartupTask TaskId='App' Enabled='false'/></d:Extension></Package>", declared)) && declared);
    REQUIRE(SUCCEEDED(InspectXml("<Package xmlns:d='http://schemas.microsoft.com/appx/manifest/desktop/windows10'>"
        "<d:Extension Category='windows.startupTask' Executable='app/Other.exe'/></Package>", declared)) && !declared);
    REQUIRE(SUCCEEDED(InspectXml("<Package><Extension Category='windows.startupTask' Executable='app/ChatGPT.exe'/></Package>", declared)) && !declared);
    REQUIRE(FAILED(InspectXml("<Package>", declared)) && !declared);
    REQUIRE(FAILED(InspectXml("<Package xmlns:u='http://schemas.microsoft.com/appx/manifest/uap/windows10/5'>"
        "<u:Extension Category='windows.startupTask' Executable='app/ChatGPT.exe'>"
        "<u:StartupTask TaskId='../Other'/></u:Extension></Package>", declared, &taskIds)) && taskIds.empty());
    REQUIRE(SUCCEEDED(InspectXml("<Package xmlns:u='http://schemas.microsoft.com/appx/manifest/uap/windows10/5'>"
        "<u:Extension Category='windows.startupTask' Executable='app/Other.exe'><u:StartupTask TaskId='Other'/></u:Extension>"
        "<u:StartupTask TaskId='Outside'/></Package>", declared, &taskIds)) && taskIds.empty());
    setup::StartupReport packaged{0, 0, 0, true};
    REQUIRE(packaged.Message().find(L"商店应用自启") != std::wstring::npos);
    REQUIRE(packaged.Message().find(L"未能确认") != std::wstring::npos);
    auto disableTask = [&]() {
        setup::StartupReport report; report.packagedTask = true;
        taskWrites = taskQueries = 0;
        DisablePackagedTasks({L"App"}, report); return report;
    };
    packaged = disableTask();
    REQUIRE(taskState == 1 && taskWrites == 1 && packaged.packagedDisabled == 1 && !packaged.packagedError);
    REQUIRE(packaged.Message().find(L"回读确认") != std::wstring::npos);
    for (DWORD off : {0u, 1u, 3u}) {
        taskState = off; packaged = disableTask();
        REQUIRE(taskState == off && !taskWrites && packaged.packagedAlreadyOff == 1 && !packaged.packagedError);
        REQUIRE(packaged.Message().find(L"已经关闭") != std::wstring::npos);
    }
    taskState = 4; packaged = disableTask();
    REQUIRE(taskState == 4 && !taskWrites && packaged.packagedError == ERROR_ACCESS_DISABLED_BY_POLICY);
    taskState = 6; packaged = disableTask();
    REQUIRE(!taskWrites && packaged.packagedError == ERROR_INVALID_DATA);
    taskState = 2; taskType = REG_BINARY; packaged = disableTask();
    REQUIRE(!taskWrites && packaged.packagedError == ERROR_INVALID_DATA);
    taskType = REG_DWORD; failTaskOpen = ERROR_FILE_NOT_FOUND; packaged = disableTask();
    REQUIRE(!taskWrites && packaged.packagedError == ERROR_FILE_NOT_FOUND);
    failTaskOpen = ERROR_ACCESS_DENIED; packaged = disableTask();
    REQUIRE(!taskWrites && packaged.packagedError == ERROR_ACCESS_DENIED);
    failTaskOpen = 0; failTaskWriteAt = 1; packaged = disableTask();
    REQUIRE(taskState == 2 && packaged.packagedError == ERROR_ACCESS_DENIED && !packaged.packagedDisabled);
    failTaskWriteAt = 0; failTaskQueryAt = 3; packaged = disableTask();
    REQUIRE(taskState == 2 && taskWrites == 2 && packaged.packagedError && !packaged.restoreError && !packaged.packagedDisabled);
    failTaskWriteAt = 2; packaged = disableTask();
    REQUIRE(taskState == 1 && packaged.restoreError == ERROR_ACCESS_DENIED && !packaged.packagedDisabled);
    failTaskQueryAt = failTaskWriteAt = 0; taskState = 2; taskRace = true; packaged = disableTask();
    REQUIRE(taskState == 3 && !taskWrites && packaged.packagedError == ERROR_RETRY);
    taskRace = false; taskState = 2; taskWrites = 0; packaged = {};
    DisablePackagedTasks({L"..\\Other"}, packaged);
    REQUIRE(!taskWrites && packaged.packagedError == ERROR_INVALID_DATA);
    const std::vector<std::wstring> known{ L"C:\\Registered App\\app\\ChatGPT.exe" };
    entries = {
        { L"official", L"\"C:\\Registered App\\app\\ChatGPT.exe\" --startup" },
        { L"same-name-other-app", L"\"C:\\Other\\ChatGPT.exe\"" },
        { L"launcher", L"C:\\Tools\\ChatGPTProxyLauncher.exe" },
        { L"argument-only", L"cmd.exe /c \"C:\\Registered App\\app\\ChatGPT.exe\"" },
        { L"unquoted-ambiguous", L"C:\\Registered App\\app\\ChatGPT.exe" }
    };
    auto result = DisableRunEntries(HKEY_CURRENT_USER, L"mock", L"mock", known);
    REQUIRE(result.matched == 1 && result.disabled == 1 && result.error == 0);
    REQUIRE(writes == 1 && approvalFixtures.size() == 1 && approvalFixtures[L"official"][0] == 3);
    REQUIRE(entries[0].command == L"\"C:\\Registered App\\app\\ChatGPT.exe\" --startup");
    REQUIRE(Matches(L"\"c:\\registered app\\APP\\CHATGPT.EXE\"", known));
    REQUIRE(!Matches(L"\"C:\\Registered App\\app\\ChatGPT.exe.evil\"", known));
    REQUIRE(!Matches(L"powershell.exe ChatGPT.exe", known));
    writes = 0;
    result = DisableRunEntries(HKEY_CURRENT_USER, L"mock", L"mock", {});
    REQUIRE(!result.disabled && !writes && result.error == ERROR_NOT_FOUND);
    failWrite = ERROR_ACCESS_DENIED;
    result = DisableRunEntries(HKEY_CURRENT_USER, L"mock", L"mock", known);
    REQUIRE(result.matched == 1 && !result.disabled && result.error == ERROR_ACCESS_DENIED);
    REQUIRE(result.Message().find(L"未能") != std::wstring::npos);
    failWrite = 0; failOpen = ERROR_ACCESS_DENIED; writes = 0;
    result = DisableRunEntries(HKEY_CURRENT_USER, L"mock", L"mock", known);
    REQUIRE(!result.disabled && !writes && result.error == ERROR_ACCESS_DENIED);
    failOpen = ERROR_FILE_NOT_FOUND;
    result = DisableRunEntries(HKEY_CURRENT_USER, L"mock", L"mock", known);
    REQUIRE(!result.matched && !result.error && !writes);
    failOpen = 0; entries.clear();
    result = DisableRunEntries(HKEY_CURRENT_USER, L"mock", L"mock", known);
    REQUIRE(!result.matched && !result.error && !writes);
    REQUIRE(result.Message().find(L"不代表自带自启仍然开启") != std::wstring::npos);
    printf("PASS startup target matching, no name-only matches, preserve Run values, registry write failures and honest results (mock registry only)\n");
    printf("PASS packaged startup manifest detection without guessing toggle state; missing/empty Run is not an error\n");
    printf("PASS packaged startup exact task selection, disable/readback, already-off no-op, unknown/policy/missing states, write/reread failures, restore and concurrent change (mock registry)\n");
    return 0;
}
