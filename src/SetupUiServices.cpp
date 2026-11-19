// SPDX-License-Identifier: GPL-3.0-only
#include "SetupUiServices.h"
#include "Localization.h"
#include "NativeMemory.h"
#include "StartupApps.h"
#include <shlobj.h>
#include <wrl/client.h>

namespace setup::ui {
namespace {
DWORD ReadPreference(PCWSTR name, DWORD fallback) {
    DWORD value = fallback, bytes = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER, l10n::PreferenceKey, name, RRF_RT_REG_DWORD,
        nullptr, &value, &bytes) != ERROR_SUCCESS) return fallback;
    return value;
}
bool WritePreference(PCWSTR name, DWORD value) {
    return RegSetKeyValueW(HKEY_CURRENT_USER, l10n::PreferenceKey, name, REG_DWORD,
        &value, sizeof(value)) == ERROR_SUCCESS;
}
}
bool ReadCodexEnvPreference() { return ReadPreference(L"ConfigureCodexEnv", 1) != 0; }
bool SaveCodexEnvPreference(bool enabled) { return WritePreference(L"ConfigureCodexEnv", enabled ? 1 : 0); }
bool ReadDarkPreference() { return ReadPreference(L"LightTheme", 0) != 1; }
bool SaveDarkPreference(bool dark) { return WritePreference(L"LightTheme", dark ? 0 : 1); }
bool SaveLanguagePreference() {
    PCWSTR value = l10n::FollowsSystem() ? L"auto" : l10n::Current().tag;
    return RegSetKeyValueW(HKEY_CURRENT_USER, l10n::PreferenceKey, L"Language", REG_SZ,
        value, static_cast<DWORD>((wcslen(value) + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
}
Result ChoosePath(HWND owner, bool program, const std::wstring& initial, std::wstring& path) {
    using Microsoft::WRL::ComPtr;
    ComPtr<IFileOpenDialog> dialog;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
    if (SUCCEEDED(hr)) {
        DWORD flags = 0;
        hr = dialog->GetOptions(&flags);
        if (SUCCEEDED(hr)) hr = dialog->SetOptions(flags | FOS_NOCHANGEDIR | FOS_DONTADDTORECENT
            | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST
            | (program ? FOS_FILEMUSTEXIST | FOS_NODEREFERENCELINKS : FOS_PICKFOLDERS));
        std::wstring supported;
        if (program) for (const auto& format : companions::Formats) {
            if (!supported.empty()) supported += L';';
            supported += L'*'; supported += format.ext;
        }
        const COMDLG_FILTERSPEC filters[] = {
            {l10n::Text("setup.filter.supported"), supported.c_str()},
            {l10n::Text("setup.filter.shortcuts"), L"*.lnk"}, {l10n::Text("setup.filter.batch"), L"*.bat;*.cmd"},
            {l10n::Text("setup.filter.powershell"), L"*.ps1"}, {l10n::Text("setup.filter.windows"), L"*.vbs;*.js;*.wsf"},
            {l10n::Text("setup.filter.python"), L"*.py;*.pyw"}, {l10n::Text("setup.filter.node"), L"*.cjs;*.mjs"},
            {l10n::Text("setup.filter.java"), L"*.jar"}, {l10n::Text("setup.filter.apps"), L"*.exe"}
        };
        if (SUCCEEDED(hr) && program) hr = dialog->SetFileTypes(ARRAYSIZE(filters), filters);
        if (SUCCEEDED(hr)) hr = dialog->SetTitle(l10n::Text(program ? "setup.dialog.program" : "setup.dialog.directory"));
        ComPtr<IShellItem> folder;
        if (!initial.empty() && SUCCEEDED(SHCreateItemFromParsingName(initial.c_str(), nullptr, IID_PPV_ARGS(&folder)))) {
            dialog->SetFolder(folder.Get());
        }
        if (SUCCEEDED(hr)) hr = dialog->Show(owner);
        if (SUCCEEDED(hr)) {
            ComPtr<IShellItem> item;
            hr = dialog->GetResult(&item);
            if (SUCCEEDED(hr)) {
                PWSTR selected = nullptr;
                hr = item->GetDisplayName(SIGDN_FILESYSPATH, &selected);
                native::TaskPtr<wchar_t> ownedPath(selected);
                if (SUCCEEDED(hr)) path = selected;
            }
        }
    }
    if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) { path.clear(); return {}; }
    return FAILED(hr) ? Result{static_cast<DWORD>(hr), ErrorText(static_cast<DWORD>(hr))} : Result{};
}
void CompleteSave(bool startup, bool codexEnv, StartupReport& report, ChatGPTCloseResult& closed) noexcept {
    if (startup) {
        try { report = TryDisableOfficialStartup(); }
        catch (...) { report.error = ERROR_NOT_ENOUGH_MEMORY; }
    }
    if (codexEnv) {
        try { closed = TryCloseChatGPT(); }
        catch (...) { closed = ChatGPTCloseResult::NeedsManualExit; }
    }
}
}
