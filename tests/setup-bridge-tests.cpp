// SPDX-License-Identifier: GPL-3.0-only
#include "MemoryFault.h"
#include "SetupBridgeFixture.h"
#include "../src/SetupBridge.h"
#include "../src/AutomationIO.h"
#include "../src/Localization.h"
#include "../src/SetupResources.h"
#include <cstdio>
#include <chrono>
#include <objbase.h>
#include <cstring>
#define REQUIRE(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); return 20; } } while (0)
using automation::Json;
using namespace setup;

int wmain(int argc, wchar_t** argv) {
    if (argc != 3) return 2;
    try {
        l10n::Select(L"zh-CN");
        const auto root = Join(argv[1], L"bridge-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
        REQUIRE(CreateDirectoryW(root.c_str(), nullptr));
        const auto paths = BridgeTestPaths(root);
        const auto input = BridgeTestPathJson(paths);
        auto read = [&](SetupBridgeBuffer& buffer) {
            return setup_bridge_read_state(reinterpret_cast<const uint8_t*>(input.data()), input.size(), &buffer);
        };
        auto parse = [](const SetupBridgeBuffer& buffer) {
            return automation::ParseJson(std::string(reinterpret_cast<const char*>(buffer.data), buffer.length), 1024 * 1024);
        };
        SetupBridgeBuffer buffer{};
        REQUIRE(read(buffer) == 0);
        auto snapshot = parse(buffer);
        REQUIRE(!snapshot.At(L"deployed").Bool() && !snapshot.At(L"hasSettings").Bool());
        REQUIRE(snapshot.At(L"options").At(L"startup").Bool() && snapshot.At(L"options").At(L"silent").Bool());
        REQUIRE(GetFileAttributesW(paths.defaultDirectory.c_str()) == INVALID_FILE_ATTRIBUTES);
        memory_fault::watchedAllocation = buffer.data; memory_fault::watchedReleased = false;
        setup_bridge_free(&buffer);
        REQUIRE(memory_fault::watchedReleased && !buffer.data && !buffer.length);
        setup_bridge_free(&buffer); setup_bridge_free(nullptr);
        REQUIRE(setup_bridge_read_state(nullptr, 1, &buffer) != 0);
        REQUIRE(setup_bridge_read_state(reinterpret_cast<const uint8_t*>("{}"), 2, &buffer) != 0);
        REQUIRE(setup_bridge_read_state(reinterpret_cast<const uint8_t*>("\xff"), 1, &buffer) != 0);
        for (int after = 0; after < 128; ++after) {
            DWORD code = 0;
            { memory_fault::Scope fail(after); code = read(buffer); }
            if (code) REQUIRE(buffer.data == nullptr && buffer.length == 0);
            setup_bridge_free(&buffer);
        }
        const auto bytes = automation::ReadBytes(argv[2], 16 * 1024 * 1024);
        const HMODULE module = GetModuleHandleW(nullptr);
        const HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(IDR_LAUNCHER), RT_RCDATA);
        REQUIRE(resource && SizeofResource(module, resource) == bytes.size());
        const auto resourceData = LockResource(LoadResource(module, resource));
        REQUIRE(resourceData && memcmp(resourceData, bytes.data(), bytes.size()) == 0);
        REQUIRE(SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)));
        Options options;
        options.directory = paths.defaultDirectory; options.proxy = true; options.codexEnv = false;
        options.silent = true; options.protocol = L"socks5"; options.port = L"7890";
        options.extra = L"\"\" \"中文 参数\" \"quote\\\"end\"";
        std::wstring target;
        REQUIRE(Apply(paths, options, {reinterpret_cast<const BYTE*>(bytes.data()), bytes.size()}, target));
        CoUninitialize();
        REQUIRE(read(buffer) == 0);
        snapshot = parse(buffer);
        REQUIRE(snapshot.At(L"deployed").Bool() && snapshot.At(L"hasSettings").Bool());
        REQUIRE(snapshot.At(L"options").At(L"extra").String() == options.extra);
        REQUIRE(snapshot.At(L"options").At(L"protocol").String() == L"socks5");
        REQUIRE(!snapshot.At(L"deployedVersion").String().empty());
        setup_bridge_free(&buffer);
        REQUIRE(GetFileAttributesW(paths.startupLink.c_str()) == INVALID_FILE_ATTRIBUTES);
        REQUIRE(GetFileAttributesW(paths.desktopLink.c_str()) == INVALID_FILE_ATTRIBUTES);
        REQUIRE(GetFileAttributesW(paths.codexEnvFile.c_str()) == INVALID_FILE_ATTRIBUTES);
        const auto begin = std::chrono::steady_clock::now();
        for (int i = 0; i < 100; ++i) { REQUIRE(read(buffer) == 0); setup_bridge_free(&buffer); }
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - begin).count();
        printf("PASS bridge: isolated load, unchanged options, exact embedded payload, same-side buffer release, 128 allocation failures; 100 deployed reads: %lld us\n", elapsed);
        return 0;
    } catch (...) { puts("FAIL bridge exception"); return 50; }
}
