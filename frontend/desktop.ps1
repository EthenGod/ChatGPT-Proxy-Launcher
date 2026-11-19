# SPDX-License-Identifier: GPL-3.0-only
param(
    [ValidateSet('build', 'dev', 'check', 'test', 'harness', 'prototype', 'sample')][string]$Command = 'build',
    [string]$RustBin = $env:CHATGPT_RUST_BIN,
    # Used by the full test pipeline after its native build has succeeded.
    [switch]$ReuseNativeBuild
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot
$previous = @{}
foreach ($name in @('PATH', 'CARGO_HOME', 'CARGO_TARGET_DIR', 'npm_config_cache', 'CHATGPT_UI_DIST', 'CHATGPT_UI_PROTOTYPE', 'TAURI_CONFIG')) {
    $previous[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
}
function Check-Exit([string]$Step) { if ($LASTEXITCODE -ne 0) { throw "$Step failed: $LASTEXITCODE" } }
try {
    if ($RustBin) { $env:PATH = [IO.Path]::GetFullPath($RustBin) + ';' + $env:PATH }
    $extraTools = Join-Path $projectRoot 'work\ui-toolchain\bin'
    if (Test-Path -LiteralPath $extraTools) { $env:PATH = $extraTools + ';' + $env:PATH }
    if (!(Get-Command cargo.exe -ErrorAction SilentlyContinue)) { throw 'Rust MSVC toolchain required. Pass -RustBin or set CHATGPT_RUST_BIN.' }
    $env:CARGO_HOME = Join-Path $projectRoot 'work\ui-toolchain\cargo'
    $env:CARGO_TARGET_DIR = Join-Path $projectRoot 'work\ui-target'
    $env:npm_config_cache = Join-Path $projectRoot 'work\npm-cache'
    $visualPreview = $Command -in @('prototype', 'sample')
    $env:CHATGPT_UI_PROTOTYPE = $(if ($visualPreview) { '1' } else { '0' })
    # A fresh asset directory avoids embedding stale bundles, without deleting files.
    $buildId = [guid]::NewGuid().ToString('N')
    $env:CHATGPT_UI_DIST = Join-Path $projectRoot ('work\ui-assets\' + $buildId)
    [IO.Directory]::CreateDirectory($env:CHATGPT_UI_DIST) | Out-Null
    # A drive-qualified path is parsed as a URL by Tauri's untagged config enum.
    $env:TAURI_CONFIG = @{build=@{frontendDist=('../../work/ui-assets/' + $buildId)}} | ConvertTo-Json -Compress
    $configFile = Join-Path $projectRoot ('work\ui-assets\' + $buildId + '.tauri.json')
    [IO.File]::WriteAllText($configFile, $env:TAURI_CONFIG)
    if (!(Test-Path -LiteralPath (Join-Path $PSScriptRoot 'node_modules\typescript'))) { throw 'Run npm ci in frontend first.' }
    Push-Location $projectRoot
    try {
        if (!$ReuseNativeBuild) { & $env:ComSpec /d /c tools\build-native.cmd; Check-Exit 'Native build' }
        foreach ($file in @('outputs\ChatGPTProxyLauncher.exe', 'work\SetupResources.res', 'work\LocalizationData.h')) {
            if (!(Test-Path -LiteralPath $file -PathType Leaf)) { throw "Native build output missing: $file" }
        }
    }
    finally { Pop-Location }
    Push-Location $PSScriptRoot
    try {
        if ($Command -eq 'test' -or $Command -eq 'check') {
            # The build script runs the TypeScript check before bundling the page.
            & npm.cmd run build; Check-Exit 'Frontend build'
            if ($Command -eq 'test') { & npm.cmd test; Check-Exit 'Frontend tests' }
            & cargo.exe fmt --manifest-path src-tauri/Cargo.toml --check; Check-Exit 'Rust format'
            & cargo.exe $Command --manifest-path src-tauri/Cargo.toml --locked --release --features custom-protocol; Check-Exit "Rust $Command"
            if ($Command -eq 'check') {
                & cargo.exe clippy --manifest-path src-tauri/Cargo.toml --locked --release --all-targets --features custom-protocol -- -D warnings; Check-Exit 'Clippy production'
                & cargo.exe clippy --manifest-path src-tauri/Cargo.toml --locked --release --all-targets --all-features -- -D warnings; Check-Exit 'Clippy test features'
            }
        } elseif ($Command -eq 'dev') {
            & npm.cmd exec -- tauri dev; Check-Exit 'Tauri dev'
        } else {
            $arguments = @('exec', '--', 'tauri', 'build', '--no-bundle', '--config', $configFile)
            if ($Command -eq 'harness') { $arguments += @('--features', 'test-harness') }
            if ($Command -eq 'prototype') { $arguments += @('--features', 'ui-prototype') }
            if ($Command -eq 'sample') { $arguments += @('--features', 'program-sample') }
            $arguments += @('--', '--locked')
            & npm.cmd @arguments; Check-Exit 'Tauri build'
            $directory = Join-Path $projectRoot $(switch ($Command) {
                'prototype' { 'work\ui-prototype' }
                'harness' { 'work\ui-stage1-test' }
                'sample' { 'work\ui-sample' }
                default { 'outputs' }
            })
            [IO.Directory]::CreateDirectory($directory) | Out-Null
            $outputName = if ($Command -eq 'sample') { 'ChatGPTLauncherSetup-Sample.exe' } else { 'ChatGPTLauncherSetup.exe' }
            $outputPath = Join-Path $directory $outputName
            Copy-Item -LiteralPath (Join-Path $env:CARGO_TARGET_DIR 'release\ChatGPTLauncherSetup.exe') -Destination $outputPath
            $metadata = @{
                kind=$Command
                prototype=$visualPreview
                testHarness=($visualPreview -or $Command -eq 'harness')
                assetsDirectory=('work/ui-assets/' + $buildId)
                setupSha256=(Get-FileHash -LiteralPath $outputPath -Algorithm SHA256).Hash
                launcherSha256=(Get-FileHash -LiteralPath (Join-Path $projectRoot 'outputs\ChatGPTProxyLauncher.exe') -Algorithm SHA256).Hash
                payloadBytes=(Get-Item -LiteralPath (Join-Path $projectRoot 'outputs\ChatGPTProxyLauncher.exe')).Length
            }
            $metadataPath = if ($Command -eq 'build') { Join-Path $projectRoot 'work\ui-build.json' } else { $directory + '-build.json' }
            [IO.File]::WriteAllText($metadataPath, ($metadata | ConvertTo-Json))
            Write-Host ('Built: ' + $outputPath)
        }
    } finally { Pop-Location }
} finally {
    foreach ($name in $previous.Keys) { [Environment]::SetEnvironmentVariable($name, $previous[$name], 'Process') }
}
