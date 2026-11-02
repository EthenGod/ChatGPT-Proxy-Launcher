# SPDX-License-Identifier: GPL-3.0-only
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$entry = Join-Path $root 'setup.ps1'
$oldTemp = $env:TEMP
$testFolder = Join-Path $root ('work\download-tests-' + [guid]::NewGuid().ToString('N'))
$null = New-Item -ItemType Directory -Path $testFolder -Force
$calls = @{launches = 0}
$fixture = [Text.Encoding]::ASCII.GetBytes('test setup payload')
$sha = [Security.Cryptography.SHA256]::Create()
try { $hash = [BitConverter]::ToString($sha.ComputeHash($fixture)).Replace('-', '').ToLowerInvariant() }
finally { $sha.Dispose() }
function Invoke-RestMethod($Uri, $TimeoutSec) {
    if ($scenario -eq 'offline') { throw 'Offline' }
    if ($Uri -ne 'https://api.github.com/repos/EthenGod/ChatGPT-Proxy-Launcher/releases/latest') { throw 'Wrong API URL' }
    @{tag_name = 'v9.8.7'}
}
function Invoke-WebRequest([switch]$UseBasicParsing, $Uri, $OutFile, $TimeoutSec) {
    $asset = Split-Path $OutFile -Leaf
    if ($Uri -ne "https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v9.8.7/$asset") { throw 'Wrong asset URL' }
    if ($asset -eq 'SHA256SUMS.txt') {
        $line = "$hash  ChatGPTLauncherSetup.exe"
        if ($scenario -eq 'missing') { $line = "$hash  Other.exe" }
        if ($scenario -eq 'duplicate') { $line += [Environment]::NewLine + $line }
        if ($scenario -eq 'malformed') { $line = 'abcd  ChatGPTLauncherSetup.exe' }
        [IO.File]::WriteAllText($OutFile, $line)
    } elseif ($asset -eq 'ChatGPTLauncherSetup.exe') {
        if ($scenario -eq 'interrupted') { throw 'Interrupted download' }
        [IO.File]::WriteAllBytes($OutFile, $fixture)
        if ($scenario -eq 'corrupt') { [IO.File]::AppendAllText($OutFile, 'x') }
    } else { throw 'Unexpected asset' }
}
function Start-Process($FilePath, $WorkingDirectory) {
    if (!(Test-Path -LiteralPath $FilePath) -or (Split-Path $FilePath) -ne $WorkingDirectory -or
        (Split-Path $WorkingDirectory) -ne $testFolder) { throw 'Unexpected launch path' }
    $calls.launches++
}
try {
    $env:TEMP = $testFolder
    foreach ($scenario in 'ok','offline','missing','duplicate','malformed','interrupted','corrupt') {
        $before = $calls.launches
        $failed = $false
        try { & $entry } catch { if ($scenario -eq 'ok') { throw }; $failed = $true }
        if ($scenario -eq 'ok') {
            if ($failed -or $calls.launches -ne $before + 1) { throw 'Valid download did not launch once' }
        } elseif (!$failed -or $calls.launches -ne $before) { throw "Failed to stop: $scenario" }
    }
    Write-Output 'PASS: version-pinned setup download, checksum validation and safe failure (mock network and launch).'
} finally { $env:TEMP = $oldTemp }
