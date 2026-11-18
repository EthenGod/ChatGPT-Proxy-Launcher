# SPDX-License-Identifier: GPL-3.0-only
# Build, test, and prepare local upload files. Does not publish or change user startup.
param([switch]$TestPackage)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$projectRoot = $PSScriptRoot
# Generate before taking source hashes; the downloader contains the same catalog subset as the EXEs.
& (Join-Path $projectRoot 'tools\locales.ps1') -Generate -ProjectRoot $projectRoot

function Get-Sha256([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '').ToLowerInvariant() }
    finally { $sha.Dispose(); $stream.Dispose() }
}

function Write-VerifiedZip([string]$Path, [System.Collections.IDictionary]$Files) {
    $zip = [IO.Compression.ZipFile]::Open($Path, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($name in $Files.Keys) {
            [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $Files[$name], $name,
                [IO.Compression.CompressionLevel]::Optimal) | Out-Null
        }
    } finally { $zip.Dispose() }
    $zip = [IO.Compression.ZipFile]::OpenRead($Path)
    try {
        if ($zip.Entries.Count -ne $Files.Count) { throw 'ZIP entry count mismatch.' }
        foreach ($entry in $zip.Entries) {
            if (!$Files.Contains($entry.FullName)) { throw 'Unexpected ZIP entry.' }
            $stream = $entry.Open()
            $sha = [Security.Cryptography.SHA256]::Create()
            try { $actual = [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '').ToLowerInvariant() }
            finally { $sha.Dispose(); $stream.Dispose() }
            if ($actual -ne (Get-Sha256 $Files[$entry.FullName])) { throw ('ZIP content mismatch: ' + $entry.FullName) }
        }
    } finally { $zip.Dispose() }
}

# Explicit list: never sweep the repository, work directory, Git data or private files.
$sourceNames = @(
    '.editorconfig', '.gitattributes', '.gitignore', 'AGENTS.md', 'HANDOFF.md', 'LICENSE', 'README.md', 'README.en.md', 'README.ja.md', 'build.cmd', 'release.cmd', 'release.ps1', 'setup.ps1',
    'tools/msvc-env.cmd', 'tools/build-native.cmd', 'tools/locales.ps1', 'locales/languages.json', 'locales/catalog.schema.json',
    'assets/ChatGPT.ico', 'assets/README.md', 'assets/README.en.md', 'assets/README.ja.md',
    'docs/launcher-review.md', 'docs/maintainability-review.md', 'docs/performance-review.md', 'docs/release-upload.md', 'docs/setup-design.md', 'docs/taskbar-repair.md',
    'docs/automation-design.md', 'docs/automation-usage.md', 'docs/automation-review.md', 'docs/companion-protocol-v1.md',
    'docs/developer-guide.md', 'docs/automation-maintenance.md', 'docs/ui-refactor.md',
    'docs/memory-thread-review.md', 'docs/localization-design.md', 'docs/translating.md',
    'docs/translating.en.md', 'docs/translating.ja.md', 'docs/taskbar-repair.en.md', 'docs/taskbar-repair.ja.md',
    'docs/release-upload.en.md', 'docs/release-upload.ja.md',
    'docs/schemas/deploy-v1.schema.json', 'docs/schemas/companion-v1.schema.json',
    'docs/schemas/companion-config-v1.schema.json', 'docs/schemas/companion-policy-v1.schema.json', 'docs/schemas/companion-reply-v1.schema.json',
    'docs/examples/deploy-v1.json', 'docs/examples/deploy-minimal-v1.json', 'docs/examples/companion-v1.json', 'docs/examples/companion-context.ps1',
    'docs/examples/companion-config-v1.json', 'docs/examples/companion-task-v1.json',
    'src/AppIcon.h', 'src/AppVersion.h', 'src/ChatGPTLauncherSetup.cpp', 'src/ChatGPTProxyLauncher.cpp',
    'src/AutomationJson.cpp', 'src/AutomationJson.h', 'src/AutomationIO.cpp', 'src/AutomationIO.h',
    'src/AutomationContract.h', 'src/DeploymentDiscovery.cpp', 'src/DeploymentDiscovery.h',
    'src/SetupCommandLine.cpp', 'src/SetupCommandLine.h', 'src/SetupBridge.cpp', 'src/SetupBridge.h',
    'src/SetupUiServices.cpp', 'src/SetupUiServices.h', 'src/SetupNative.h', 'src/SetupUiCapabilities.json',
    'src/CompanionManifest.cpp', 'src/CompanionManifest.h', 'src/CompanionRegistry.cpp',
    'src/CompanionProtocol.cpp', 'src/CompanionProtocol.h',
    'src/CompanionRunner.cpp', 'src/CompanionRunner.h', 'src/LauncherAutomation.cpp', 'src/LauncherAutomation.h', 'src/Launcher.manifest',
    'src/CodexClose.cpp', 'src/CodexClose.h',
    'src/CodexEnv.cpp', 'src/CodexEnv.h',
    'src/NativeMemory.h', 'src/Localization.cpp', 'src/Localization.h',
    'src/LauncherResources.rc', 'src/ProxyDetection.cpp', 'src/ProxyDetection.h',
    'src/Setup.manifest', 'src/SetupArguments.cpp', 'src/SetupCore.cpp', 'src/SetupCore.h',
    'src/SetupResources.h', 'src/SetupResources.rc', 'src/StartupApps.cpp', 'src/StartupApps.h',
    'src/StartupIntegration.cpp', 'src/StartupIntegration.h', 'src/TaskbarShortcut.cpp', 'src/TaskbarShortcut.h',
    'tests/analyze.cmd', 'tests/codex-close-tests.cpp', 'tests/companion-tests.cpp', 'tests/download-tests.ps1',
    'tests/automation-tests.cpp', 'tests/chain-tests.cpp', 'tests/headless-download-tests.ps1', 'tests/protocol-example-tests.ps1',
    'tests/contract-tests.cpp', 'tests/localization-tests.cpp', 'tests/localization-tests.ps1', 'tests/documentation-tests.ps1',
    'tests/memory-tests.cpp', 'tests/MemoryFault.h', 'tests/file-tests.cpp',
    'tests/launcher-tests.cpp', 'tests/preview-download.ps1', 'tests/proxy-tests.cpp',
    'tests/setup-tests.cpp', 'tests/setup-ui-tests.cpp', 'tests/startup-tests.cpp',
    'tests/taskbar-tests.cpp', 'tests/test-build.cmd',
    'tests/SetupBridgeFixture.h', 'tests/setup-bridge-tests.cpp', 'tests/setup-bridge-actions-tests.cpp', 'tests/setup-bridge-runner.cpp', 'tests/test-bridge.cmd', 'tests/ui-stage1-tests.ps1',
    'tests/webview-download-tests.ps1', 'tests/ui-prototype-tests.ps1', 'tests/ui-interaction-checks.js', 'tests/ui-accessibility-checks.js', 'tests/delivery-tests.ps1',
    'tests/fixtures/setup-snapshot.json', 'tests/frontend/contract.test.ts', 'tests/frontend/desktop-service.test.ts', 'tests/frontend/interaction.test.tsx', 'tests/frontend/appearance.test.tsx', 'tests/frontend/errors.test.tsx',
    'tests/rust/bridge_contract.rs', 'tests/rust/embedded_assets.rs',
    'frontend/.npmrc', 'frontend/.cargo/config.toml', 'frontend/desktop.ps1', 'frontend/preview.ps1', 'frontend/index.html', 'frontend/prototype.html',
    'frontend/package.json', 'frontend/package-lock.json', 'frontend/tsconfig.json', 'frontend/vite.config.ts',
    'frontend/src/main.tsx', 'frontend/src/prototype.tsx', 'frontend/src/app/App.tsx', 'frontend/src/i18n/index.ts',
    'frontend/src/components/controls.tsx', 'frontend/src/components/ScrollBox.tsx', 'frontend/src/components/useAppearanceTransition.ts', 'frontend/src/components/NoticeToasts.tsx', 'frontend/src/components/ErrorDetails.tsx', 'frontend/src/features/setup/SetupPage.tsx',
    'frontend/src/features/setup/contract.ts', 'frontend/src/features/setup/preview.ts', 'frontend/src/features/setup/errors.ts', 'frontend/src/features/setup/SetupSkeleton.tsx', 'frontend/src/features/setup/SetupStatus.tsx',
    'frontend/src/platform/desktop.ts', 'frontend/src/platform/setup-service.ts', 'frontend/src/platform/types.ts', 'frontend/src/styles/theme.css',
    'frontend/src/platform/mock.ts', 'frontend/src/platform/prototype-window.ts', 'frontend/src/styles/prototype.css',
    'frontend/src-tauri/Cargo.toml', 'frontend/src-tauri/Cargo.lock', 'frontend/src-tauri/build.rs',
    'frontend/src-tauri/tauri.conf.json', 'frontend/src-tauri/capabilities/setup-readonly.json',
    'frontend/src-tauri/src/main.rs', 'frontend/src-tauri/src/bootstrap.rs', 'frontend/src-tauri/src/bridge.rs',
    'frontend/src-tauri/src/model.rs', 'frontend/src-tauri/src/harness.rs'
)
# Only explicitly registered catalogs enter the source package; never sweep arbitrary JSON files.
$languages = ConvertFrom-Json ([IO.File]::ReadAllText((Join-Path $projectRoot 'locales\languages.json')))
foreach ($language in $languages.languages) { $sourceNames += 'locales/' + $language.tag + '.json' }
$sources = [ordered]@{}
$sourceHashes = @{}
foreach ($name in $sourceNames) {
    $path = Join-Path $projectRoot $name
    if (!(Test-Path -LiteralPath $path -PathType Leaf)) { throw ('Missing release source: ' + $name) }
    $sources['ChatGPTLauncher-source/' + $name] = $path
    $sourceHashes[$name] = Get-Sha256 $path
}

Push-Location $projectRoot
try {
    & $env:ComSpec /d /c tests\analyze.cmd
    if ($LASTEXITCODE -ne 0) { throw ('Static analysis failed with exit code ' + $LASTEXITCODE + '. No release files updated.') }
    & $env:ComSpec /d /c tests\test-build.cmd
    if ($LASTEXITCODE -ne 0) { throw ('Build/tests failed with exit code ' + $LASTEXITCODE + '. No release files updated.') }
    foreach ($name in $sourceNames) {
        if ((Get-Sha256 (Join-Path $projectRoot $name)) -ne $sourceHashes[$name]) {
            throw ('Source changed during build: ' + $name + '. Run release.cmd again.')
        }
    }

    # Unique staging directories are kept for inspection; no bulk deletion or cleanup.
    $stage = Join-Path $projectRoot ('work\release-stage-' + [Guid]::NewGuid().ToString('N'))
    [IO.Directory]::CreateDirectory($stage) | Out-Null
    $setupPath = Join-Path $projectRoot 'outputs\ChatGPTLauncherSetup.exe'
    $releaseVersion = [Diagnostics.FileVersionInfo]::GetVersionInfo($setupPath).FileVersion
    if (!$releaseVersion -or $releaseVersion -notmatch '^\d+\.\d+\.\d+(\.\d+)?$') {
        throw 'The built setup executable must have a numeric release version.'
    }
    $releaseDir = Join-Path (Join-Path $projectRoot 'Releases') ('v' + $releaseVersion)
    $binaryNames = @('ChatGPTLauncherSetup.exe', 'ChatGPTProxyLauncher.exe')
    foreach ($name in $binaryNames) {
        $built = Join-Path $projectRoot ('outputs\' + $name)
        $binaryVersion = [Diagnostics.FileVersionInfo]::GetVersionInfo($built).FileVersion
        if ($binaryVersion -ne $releaseVersion) { throw ('Release binary version mismatch: ' + $name) }
        $staged = Join-Path $stage $name
        [IO.File]::Copy($built, $staged, $false)
        if ((Get-Sha256 $built) -ne (Get-Sha256 $staged)) { throw 'Staged binary mismatch.' }
    }
    Write-VerifiedZip (Join-Path $stage 'ChatGPTLauncher-source.zip') $sources
    foreach ($name in $sourceNames) {
        if ((Get-Sha256 (Join-Path $projectRoot $name)) -ne $sourceHashes[$name]) { throw 'Source changed while packaging.' }
    }

    $uploads = @('ChatGPTLauncherSetup.exe', 'ChatGPTProxyLauncher.exe', 'ChatGPTLauncher-source.zip')
    $checksums = foreach ($name in $uploads) { (Get-Sha256 (Join-Path $stage $name)) + '  ' + $name }
    [IO.File]::WriteAllLines((Join-Path $stage 'SHA256SUMS.txt'), [string[]]$checksums, [Text.Encoding]::ASCII)
    & (Join-Path $projectRoot 'tests\delivery-tests.ps1') -PackageDirectory $stage
    if ($TestPackage) {
        Write-Host ('Local test package: ' + $stage + '. Existing releases were not changed. Nothing was uploaded.')
        return
    }
    [IO.Directory]::CreateDirectory($releaseDir) | Out-Null
    foreach ($name in ($uploads + @('SHA256SUMS.txt'))) {
        $source = Join-Path $stage $name
        $destination = Join-Path $releaseDir $name
        [IO.File]::Copy($source, $destination, $true)
        if ((Get-Sha256 $source) -ne (Get-Sha256 $destination)) { throw ('Release copy mismatch: ' + $name) }
    }
    [IO.File]::Copy((Join-Path $projectRoot 'docs\release-upload.md'), (Join-Path $projectRoot 'Releases\README.md'), $true)
    Write-Host ('Release files are ready in Releases\v' + $releaseVersion + '. Nothing was uploaded.')
} finally { Pop-Location }
