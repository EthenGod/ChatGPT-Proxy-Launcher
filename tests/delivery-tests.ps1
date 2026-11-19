# SPDX-License-Identifier: GPL-3.0-only
param([string]$PackageDirectory, [hashtable]$SourceHashes)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$projectRoot = Split-Path $PSScriptRoot
function Require([bool]$Value, [string]$Message) { if (!$Value) { throw $Message } }
function Hash([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }

$names = @(& (Join-Path $projectRoot 'tools/release-sources.ps1') -ProjectRoot $projectRoot)

$metadata=Get-Content -LiteralPath (Join-Path $projectRoot 'work/ui-build.json') -Raw | ConvertFrom-Json
Require ($metadata.kind -eq 'build' -and !$metadata.testHarness -and !$metadata.prototype) 'Production output came from a test build'
$assets=Join-Path $projectRoot $metadata.assetsDirectory
Require ([IO.File]::Exists((Join-Path $assets 'index.html')) -and ![IO.File]::Exists((Join-Path $assets 'prototype.html'))) 'Production assets must contain only the operational entry'
$setup=Join-Path $projectRoot 'outputs/ChatGPTLauncherSetup.exe'
$launcher=Join-Path $projectRoot 'outputs/ChatGPTProxyLauncher.exe'
Require ((Hash $setup) -eq $metadata.setupSha256 -and (Hash $launcher) -eq $metadata.launcherSha256) 'Outputs no longer match the same build'
$setupVersion=[Diagnostics.FileVersionInfo]::GetVersionInfo($setup).FileVersion
Require ($setupVersion -eq [Diagnostics.FileVersionInfo]::GetVersionInfo($launcher).FileVersion) 'EXE versions differ'
$packageVersion=(Get-Content -LiteralPath (Join-Path $projectRoot 'frontend/package.json') -Raw | ConvertFrom-Json).version
Require ($setupVersion -ceq $packageVersion) 'Release label differs from package version'
foreach ($pair in @(@($setup,'Setup'),@($launcher,'Launcher'))) {
    $info=[Diagnostics.FileVersionInfo]::GetVersionInfo($pair[0])
    $fixed='{0}.{1}.{2}.{3}' -f $info.FileMajorPart,$info.FileMinorPart,$info.FileBuildPart,$info.FilePrivatePart
    $manifest=[xml][IO.File]::ReadAllText((Join-Path $projectRoot ('src/'+$pair[1]+'.manifest')))
    Require ($fixed -ceq $manifest.assembly.assemblyIdentity.version -and $info.ProductVersion -ceq $packageVersion) 'Fixed or product version mismatch'
}
$binaryText=[Text.Encoding]::UTF8.GetString([IO.File]::ReadAllBytes($setup))
foreach($marker in @('CHATGPT_SETUP_TEST_ROOT','CHATGPT_SETUP_TEST_SAVE_DELAY_MS','test harness requires an explicit root')) {
    Require (!$binaryText.Contains($marker)) "Production EXE contains test code: $marker"
}

function Read-Imports([string]$Path) {
    $bytes=[IO.File]::ReadAllBytes($Path)
    $pe=[BitConverter]::ToInt32($bytes,0x3c)
    Require ([BitConverter]::ToUInt32($bytes,$pe) -eq 0x4550 -and [BitConverter]::ToUInt16($bytes,$pe+4) -eq 0x8664) 'Expected a Windows x64 PE'
    $optional=$pe+24
    Require ([BitConverter]::ToUInt16($bytes,$optional) -eq 0x20b) 'Expected PE32+'
    $sections=$optional+[BitConverter]::ToUInt16($bytes,$pe+20)
    $sectionCount=[BitConverter]::ToUInt16($bytes,$pe+6)
    function Offset([uint32]$Rva) {
        for($index=0;$index -lt $sectionCount;$index++) {
            $section=$sections+40*$index
            $start=[BitConverter]::ToUInt32($bytes,$section+12)
            $size=[Math]::Max([BitConverter]::ToUInt32($bytes,$section+8),[BitConverter]::ToUInt32($bytes,$section+16))
            if($Rva -ge $start -and $Rva -lt $start+$size) { return [int]([BitConverter]::ToUInt32($bytes,$section+20)+$Rva-$start) }
        }
        throw 'Invalid import-table address'
    }
    $importRva=[BitConverter]::ToUInt32($bytes,$optional+120)
    if(!$importRva) { return }
    $entry=Offset $importRva
    while($nameRva=[BitConverter]::ToUInt32($bytes,$entry+12)) {
        $start=Offset $nameRva; $end=$start
        while($bytes[$end]) { $end++ }
        [Text.Encoding]::ASCII.GetString($bytes,$start,$end-$start)
        $entry+=20
    }
}
foreach($exe in @($setup,$launcher)) {
    $imports=@(Read-Imports $exe)
    # UCRT/API-set imports are Windows 10+ system components. Reject redistributable
    # VC++ libraries and a separate WebView loader, not the OS-provided UCRT.
    Require (@($imports | Where-Object { $_ -match '^(vcruntime|msvcp|msvcr\d|ucrtbased|WebView2Loader)' }).Count -eq 0) 'EXE requires a separate C++ runtime or WebView loader DLL'
    if($exe -eq $launcher) { Require (@($imports | Where-Object { $_ -match 'webview|tauri' }).Count -eq 0) 'Standalone launcher gained a frontend dependency' }
}

$capability=Get-Content -LiteralPath (Join-Path $projectRoot 'frontend/src-tauri/capabilities/main-window.json') -Raw | ConvertFrom-Json
$config=Get-Content -LiteralPath (Join-Path $projectRoot 'frontend/src-tauri/tauri.conf.json') -Raw | ConvertFrom-Json
$enabled=@($config.app.security.capabilities)
Require ($enabled.Count -eq 1 -and $enabled[0] -ceq $capability.identifier) 'Desktop configuration must select the main window capability'
Require (($capability.windows -join ',') -ceq 'main' -and !($capability.PSObject.Properties.Name -contains 'remote')) 'Desktop commands must be limited to the local main window'
Write-Host "PASS delivery: $($names.Count) source files, production features/assets, same-build EXEs, no redistributable DLLs and local permissions"

if($PackageDirectory) {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip=[IO.Compression.ZipFile]::OpenRead((Join-Path $PackageDirectory 'ChatGPTLauncher-source.zip'))
    try {
        Require ($zip.Entries.Count -eq $names.Count) 'Source ZIP entry count differs'
        $remaining = [Collections.Generic.Dictionary[string,string]]::new([StringComparer]::Ordinal)
        foreach ($name in $names) {
            $expected = if ($SourceHashes) { $SourceHashes[$name] } else { Hash (Join-Path $projectRoot $name) }
            $remaining.Add('ChatGPTLauncher-source/' + $name, $expected)
        }
        foreach($entry in $zip.Entries) {
            $key = $entry.FullName
            Require ($remaining.ContainsKey($key)) "Unexpected or duplicate source ZIP entry: $key"
            $stream = $entry.Open()
            try { $hash = (Get-FileHash -InputStream $stream -Algorithm SHA256).Hash }
            finally { $stream.Dispose() }
            Require ($hash -eq $remaining[$key]) "Source ZIP content differs: $key"
            $null = $remaining.Remove($key)
        }
        Require ($remaining.Count -eq 0) 'Missing source ZIP entry'
    } finally { $zip.Dispose() }
    $checksums=@(Get-Content -LiteralPath (Join-Path $PackageDirectory 'SHA256SUMS.txt'))
    Require ($checksums.Count -eq 3) 'Unexpected checksum count'
    foreach($name in @('ChatGPTLauncherSetup.exe','ChatGPTProxyLauncher.exe','ChatGPTLauncher-source.zip')) {
        Require ($checksums -ccontains ((Hash (Join-Path $PackageDirectory $name))+'  '+$name)) "Package checksum mismatch: $name"
    }
    foreach($name in @('ChatGPTLauncherSetup.exe','ChatGPTProxyLauncher.exe')) {
        Require ((Hash (Join-Path $PackageDirectory $name)) -eq (Hash (Join-Path $projectRoot ('outputs/'+$name)))) "Packaged binary differs: $name"
    }
    Write-Host 'PASS local package: both EXEs, exact source allowlist and SHA-256 checksums'
}
