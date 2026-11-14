// SPDX-License-Identifier: GPL-3.0-only
#include "../src/CompanionManifest.h"
#include "../src/DeploymentDiscovery.h"
#include <stdio.h>
#include <set>

// Cross-file contracts: catch schema/example drift without adding a runtime schema engine.
using automation::Json;
namespace rules = automation::contract;
#define REQUIRE(x) do { if (!(x)) { printf("FAIL contract line %d: %s\n", __LINE__, #x); return 20; } } while (0)
static long long Number(const Json& object, const wchar_t* key) { return object.At(key).Integer(0, MAXDWORD); }
static bool Range(const Json& value, unsigned low, unsigned high) { return Number(value, L"minimum") == low && Number(value, L"maximum") == high; }
int wmain(int argc, wchar_t** argv) {
    if (argc != 2) return 2;
    try {
        auto read = [&](PCWSTR path) { return automation::ParseJson(automation::ReadBytes(chain::Join(argv[1], path))); };
        const auto manifest = read(L"docs\\schemas\\companion-v1.schema.json");
        const auto& fields = manifest.At(L"properties");
        REQUIRE(Number(fields.At(L"schemaVersion"), L"const") == rules::ManifestSchema);
        REQUIRE(Number(fields.At(L"requires"), L"maxItems") == rules::Capabilities);
        REQUIRE(Number(fields.At(L"dependsOn"), L"maxItems") + 1 == rules::Entries);
        const auto& args = manifest.At(L"$defs").At(L"arguments");
        REQUIRE(Number(args, L"maxItems") == rules::Arguments && Number(args.At(L"items"), L"maxLength") == rules::ArgumentChars);
        std::set<std::wstring> capabilities;
        for (const auto& cap : fields.At(L"requires").At(L"items").At(L"enum").array) capabilities.insert(cap.String());
        size_t count = 0;
        for (const auto& cap : rules::CapabilityTable) if (cap.manifest) { REQUIRE(capabilities.count(cap.name) == 1); ++count; }
        REQUIRE(capabilities.size() == count);
        for (const auto& variant : fields.At(L"readiness").At(L"oneOf").array) {
            const auto& timeout = variant.At(L"properties").At(L"timeoutMs");
            REQUIRE(Range(timeout, rules::TimeoutMinMs, rules::TimeoutMaxMs) && Number(timeout, L"default") == rules::TimeoutDefaultMs);
        }
        const auto& retry = fields.At(L"retry").At(L"properties");
        REQUIRE(Range(retry.At(L"maxAttempts"), 1, rules::AttemptsMax));
        REQUIRE(Range(retry.At(L"delayMs"), rules::RetryMinMs, rules::RetryMaxMs) && Number(retry.At(L"delayMs"), L"default") == rules::RetryDefaultMs);
        const auto config = read(L"docs\\schemas\\companion-config-v1.schema.json");
        REQUIRE(Number(config.At(L"properties").At(L"schemaVersion"), L"const") == rules::SetupApi);
        const auto& overrides = config.At(L"properties").At(L"overrides").At(L"properties");
        REQUIRE(Range(overrides.At(L"timeoutMs"), rules::TimeoutMinMs, rules::TimeoutMaxMs));
        REQUIRE(Range(overrides.At(L"maxAttempts"), 1, rules::AttemptsMax));
        const auto policy = read(L"docs\\schemas\\companion-policy-v1.schema.json");
        REQUIRE(Number(policy.At(L"properties").At(L"schemaVersion"), L"const") == rules::PolicySchema);
        const auto& budget = policy.At(L"properties").At(L"budgetMs");
        REQUIRE(Range(budget, rules::BudgetMinMs, rules::BudgetMaxMs) && Number(budget, L"default") == rules::BudgetDefaultMs);
        const auto reply = read(L"docs\\schemas\\companion-reply-v1.schema.json");
        REQUIRE(Number(reply.At(L"properties").At(L"schemaVersion"), L"const") == rules::ReplySchema);
        const auto deploy = read(L"docs\\schemas\\deploy-v1.schema.json");
        REQUIRE(Number(deploy.At(L"properties").At(L"schemaVersion"), L"const") == rules::SetupApi);
        const auto example = read(L"docs\\examples\\companion-v1.json");
        REQUIRE(Number(example, L"schemaVersion") == rules::ManifestSchema && chain::ValidId(example.At(L"id").String()));
        for (const auto& cap : example.At(L"requires").array) REQUIRE(rules::SupportsManifestCapability(cap.String()));
        const auto configure = read(L"docs\\examples\\companion-config-v1.json");
        REQUIRE(Number(configure, L"schemaVersion") == rules::SetupApi && configure.At(L"id").String() == example.At(L"id").String());
        chain::ValidateOverrides(configure.At(L"overrides"));
        REQUIRE(Number(read(L"docs\\examples\\deploy-v1.json"), L"schemaVersion") == rules::SetupApi);
        const auto discovery = automation::MakeDiscoveryRecord(L"C:\\Example\\ChatGPTProxyLauncher.exe", L"1.0.0");
        REQUIRE(Number(discovery, L"schemaVersion") == rules::DiscoverySchema);
        REQUIRE(discovery.At(L"setupApiVersions").array[0].Integer(1, MAXDWORD) == rules::SetupApi);
        REQUIRE(discovery.At(L"chainProtocolVersions").array[0].Integer(1, MAXDWORD) == rules::ChainProtocol);
        puts("PASS contract: native constants, capability table, schema limits and examples agree");
        return 0;
    } catch (const std::exception& e) { printf("FAIL contract: %s\n", e.what()); }
    catch (const automation::IoError& e) { printf("FAIL contract I/O: %lu\n", e.code); }
    return 20;
}
