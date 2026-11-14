// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstddef>
#include <string_view>

// Wire names and limits live here. Product versions stay in AppVersion.h.
// A breaking change increments only the affected schema/protocol version.
namespace automation::contract {
inline constexpr int SetupApi = 1, ManifestSchema = 1, RegistrySchema = 1;
inline constexpr int DiscoverySchema = 1, ContextSchema = 1, ReplySchema = 1, PolicySchema = 1, ChainProtocol = 1;
inline constexpr size_t JsonBytes = 64 * 1024, ReplyBytes = 4096, PolicyBytes = 4096;
inline constexpr unsigned JsonDepth = 16, OutputDepth = 20;
inline constexpr size_t Entries = 32, Arguments = 128, ArgumentChars = 8192, Capabilities = 16;
inline constexpr unsigned TimeoutMinMs = 1000, TimeoutMaxMs = 30000, TimeoutDefaultMs = 15000;
inline constexpr unsigned StabilityMs = 1000, AttemptsMax = 3;
inline constexpr unsigned RetryMinMs = 250, RetryMaxMs = 5000, RetryDefaultMs = 1000;
inline constexpr unsigned BudgetMinMs = 1000, BudgetMaxMs = 120000, BudgetDefaultMs = 60000;
inline constexpr unsigned JobPollMs = 250, NotificationMs = 10000, LockWaitMs = 5000;
inline constexpr wchar_t ManagedBy[] = L"ChatGPTProxyLauncher";
inline constexpr wchar_t DiscoveryFile[] = L"discovery.json", PolicyFile[] = L"companion-policy.json";
inline constexpr wchar_t RegistryFolder[] = L"CompanionRegistry", SettingsLink[] = L"Launcher settings.lnk";
inline constexpr wchar_t Guard[] = L"CHATGPT_PROXY_LAUNCHER_COMPANION";
inline constexpr wchar_t ReadyCapability[] = L"ready-file-v1", RetryCapability[] = L"tracked-retry-v1";
struct Capability { const wchar_t* name; bool launcher; bool manifest; };
inline constexpr Capability CapabilityTable[] = {
    {L"headless-deploy-v1", false, false},
    {L"companion-registry-v1", false, true},
    {ReadyCapability, true, true},
    {RetryCapability, true, true},
    {L"chain-run-result-v1", true, true}
};
inline bool SupportsManifestCapability(std::wstring_view name) {
    for (const auto& capability : CapabilityTable) if (capability.manifest && name == capability.name) return true;
    return false;
}
inline bool DependencyReady(std::wstring_view status) { return status == L"ready" || status == L"already-running" || status == L"completed"; }
inline bool ItemSucceeded(std::wstring_view status) { return DependencyReady(status) || status == L"started-unverified" || status == L"disabled"; }
inline bool PermanentError(std::wstring_view code) {
    return code == L"invalid-config" || code == L"invalid-arguments" || code == L"permission-denied"
        || code == L"runtime-missing" || code == L"unsupported-capability";
}
}
