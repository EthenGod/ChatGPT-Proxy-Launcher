# SPDX-License-Identifier: GPL-3.0-only
# Build, test, and prepare local upload files. Does not publish or change user startup.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$projectRoot = $PSScriptRoot

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
    '.gitignore', 'AGENTS.md', 'LICENSE', 'README.md', 'build.cmd', 'release.cmd', 'release.ps1',
    'assets/ChatGPT.ico', 'assets/README.md', 'docs/setup-design.md', 'docs/launcher-review.md', 'docs/release-upload.md',
    'src/AppIcon.h', 'src/ChatGPTLauncherSetup.cpp', 'src/ChatGPTProxyLauncher.cpp',
    'src/LauncherResources.rc', 'src/ProxyDetection.cpp', 'src/ProxyDetection.h',
    'src/Setup.manifest', 'src/SetupCore.cpp', 'src/SetupCore.h',
    'src/SetupResources.h', 'src/SetupResources.rc', 'src/StartupIntegration.cpp', 'src/StartupIntegration.h',
    'tests/proxy-tests.cpp', 'tests/setup-tests.cpp', 'tests/setup-ui-tests.cpp', 'tests/startup-tests.cpp',
    'work/launcher-tests.cpp', 'work/test-build.cmd'
)
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
    & $env:ComSpec /d /c work\test-build.cmd
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
