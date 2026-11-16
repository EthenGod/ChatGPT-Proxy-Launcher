// SPDX-License-Identifier: GPL-3.0-only
#include "SetupBridge.h"
#include "SetupCommandLine.h"
#include "SetupResources.h"
#include "Localization.h"
#include "AppVersion.h"
#include <objbase.h>
#include <cstring>
#include <memory>
#include <new>

namespace {
using automation::Json;
constexpr size_t MaxResponseBytes = 1024 * 1024;

setup::Result ResolvePaths(const uint8_t* bytes, size_t size, setup::Paths& paths) {
    if (!bytes && !size) return setup::GetPaths(paths);
    if (!bytes || !size || size > automation::contract::JsonBytes) throw automation::Invalid("invalid-paths");
    const auto value = automation::ParseJson(std::string(reinterpret_cast<const char*>(bytes), size));
    value.Fields({L"defaultDirectory", L"startupLink", L"settingsLink", L"desktopLink", L"codexEnvFile",
        L"legacyStartupLink", L"legacyDesktopLink"});
    auto get = [&](PCWSTR key) {
        const auto& path = value.At(key).String();
        if (path.empty()) throw automation::Invalid("missing-isolated-path");
        return automation::FullPath(path);
    };
    paths = {get(L"defaultDirectory"), get(L"startupLink"), get(L"settingsLink"),
        get(L"desktopLink"), get(L"codexEnvFile"), get(L"legacyStartupLink"), get(L"legacyDesktopLink")};
    return {};
}

DWORD ReadPreference(PCWSTR name, DWORD fallback) noexcept {
    DWORD value = fallback, bytes = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER, l10n::PreferenceKey, name, RRF_RT_REG_DWORD,
        nullptr, &value, &bytes) != ERROR_SUCCESS) value = fallback;
    return value;
}

void Deliver(const Json& value, SetupBridgeBuffer& output) {
    const auto bytes = automation::WriteJson(value);
    if (bytes.size() > MaxResponseBytes) throw automation::Invalid("bridge-response-too-large");
    auto memory = std::make_unique<uint8_t[]>(bytes.size());
    std::memcpy(memory.get(), bytes.data(), bytes.size());
    output.length = bytes.size();
    output.data = memory.release();
}

Json Options(const setup::Options& o) {
    auto j = Json::Object();
    j[L"startup"] = Json(o.startup); j[L"silent"] = Json(o.silent);
    j[L"proxy"] = Json(o.proxy); j[L"desktop"] = Json(o.desktop); j[L"codexEnv"] = Json(o.codexEnv);
    j[L"protocol"] = Json(o.protocol); j[L"host"] = Json(o.host); j[L"port"] = Json(o.port);
    j[L"bypass"] = Json(o.bypass); j[L"extra"] = Json(o.extra); j[L"directory"] = Json(o.directory);
    return j;
}

// Isolated CLI tests never inspect or change real Windows startup settings.
setup::StartupReport IsolatedStartup() { return {}; }
}

extern "C" void setup_bridge_initialize() noexcept { l10n::Initialize(); }

extern "C" void setup_bridge_free(SetupBridgeBuffer* buffer) noexcept {
    if (!buffer) return;
    delete[] buffer->data;
    *buffer = {};
}

extern "C" uint32_t setup_bridge_read_state(const uint8_t* pathBytes, size_t size,
    SetupBridgeBuffer* output) noexcept {
    if (!output || output->data || output->length) return ERROR_INVALID_PARAMETER;
    try {
        setup::Paths paths;
        const auto pathsResult = ResolvePaths(pathBytes, size, paths);
        if (!pathsResult) return pathsResult.code;
        const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (FAILED(com)) return static_cast<uint32_t>(com);
        struct Apartment { ~Apartment() { CoUninitialize(); } } apartment;
        setup::Loaded loaded;
        const auto status = setup::Load(paths, loaded);
        if (!status) {
            auto error = Json::Object();
            error[L"nativeError"] = Json::Number(status.code);
            error[L"message"] = Json(status.message);
            error[L"commit"] = Json(L"not-started");
            Deliver(error, *output);
            return status.code;
        }
        // Test paths imply deterministic preferences; production never supplies them.
        const bool isolated = pathBytes != nullptr;
        loaded.options.codexEnv = isolated || ReadPreference(L"ConfigureCodexEnv", 1) != 0;
        auto state = Json::Object();
        state[L"schemaVersion"] = Json::Number(1);
        state[L"setupVersion"] = Json(L"" APP_VERSION_TEXT);
        state[L"deployedVersion"] = Json(loaded.deployedVersion);
        state[L"deployed"] = Json(loaded.deployed);
        state[L"hasSettings"] = Json(loaded.hasSettings);
        state[L"validArguments"] = Json(loaded.validArguments);
        state[L"notice"] = Json(loaded.notice);
        state[L"options"] = Options(loaded.options);
        auto preferences = Json::Object();
        preferences[L"dark"] = Json(isolated || ReadPreference(L"LightTheme", 0) != 1);
        preferences[L"language"] = Json(l10n::FollowsSystem() ? L"auto" : l10n::Current().tag);
        preferences[L"resolvedLanguage"] = Json(l10n::Current().tag);
        state[L"preferences"] = std::move(preferences);
        const HMODULE module = GetModuleHandleW(nullptr);
        const HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(IDR_LAUNCHER), RT_RCDATA);
        state[L"payloadBytes"] = Json::Number(resource ? SizeofResource(module, resource) : 0);
        Deliver(state, *output);
        return ERROR_SUCCESS;
    } catch (const std::bad_alloc&) { return ERROR_NOT_ENOUGH_MEMORY; }
    catch (const automation::IoError& error) { return error.code; }
    catch (...) { return ERROR_INVALID_DATA; }
}

extern "C" int32_t setup_bridge_headless(const uint16_t* tail, size_t tailLength,
    const uint8_t* pathBytes, size_t pathsLength) noexcept {
    static_assert(sizeof(wchar_t) == sizeof(uint16_t), "Windows UTF-16 ABI required");
    if (!tail || !tailLength || tailLength > 32766) return 20;
    try {
        const std::wstring arguments(reinterpret_cast<const wchar_t*>(tail), tailLength);
        if (arguments.find(L'\0') != arguments.npos) return 20;
        setup::Paths paths;
        if (pathBytes || pathsLength) {
            if (!ResolvePaths(pathBytes, pathsLength, paths)) return 50;
            return setup::RunSetupCommandLine(GetModuleHandleW(nullptr), arguments, &paths, IsolatedStartup);
        }
        return setup::RunSetupCommandLine(GetModuleHandleW(nullptr), arguments);
    } catch (...) { return 50; }
}

extern "C" void setup_bridge_show_bootstrap_error(uint32_t webviewMissing) noexcept {
    MessageBoxW(nullptr, l10n::Text(webviewMissing ? "setup.error.webviewmissing" : "setup.error.webviewstart"),
        l10n::Text("setup.error.open"), MB_OK | MB_ICONERROR);
}
