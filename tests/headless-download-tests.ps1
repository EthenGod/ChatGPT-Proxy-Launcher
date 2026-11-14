# SPDX-License-Identifier: GPL-3.0-only
param([string]$ProjectRoot = (Split-Path $PSScriptRoot))
$ErrorActionPreference = 'Stop'
$testRoot = Join-Path $ProjectRoot ('work\headless-download-' + [guid]::NewGuid().ToString('N'))
$null = New-Item -ItemType Directory -Path $testRoot
$requestFile = Join-Path $testRoot 'request.json'
[IO.File]::WriteAllText($requestFile, '{"schemaVersion":1}')
$fixture = [Text.Encoding]::ASCII.GetBytes('mock setup')
$sha = [Security.Cryptography.SHA256]::Create()
try { $hash = [BitConverter]::ToString($sha.ComputeHash($fixture)).Replace('-','') } finally { $sha.Dispose() }
$calls = @{launch=0; folder=$null; version='1.0.7'; arguments=$null}
function Write-Host { throw 'Headless wrote to the host' }
function Write-Progress { throw 'Headless showed progress' }
function Start-Sleep { throw 'Headless waited for completion text' }
function Invoke-RestMethod($Uri, $TimeoutSec) {
    if ($ProgressPreference -ne 'SilentlyContinue') { throw 'Headless progress enabled' }
    if ($scenario -eq 'offline') { throw 'offline' }
    return @{tag_name='v1.0.7'}
}
function Invoke-WebRequest([switch]$UseBasicParsing, $Uri, $OutFile, $TimeoutSec) {
    $calls.folder = Split-Path $OutFile
    if ($Uri -notlike 'https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.0.7/*') { throw 'Tag changed' }
    if ($OutFile.EndsWith('.txt')) { [IO.File]::WriteAllText($OutFile, "$hash  ChatGPTLauncherSetup.exe") }
    else { [IO.File]::WriteAllBytes($OutFile, $fixture); if ($scenario -eq 'checksum') { [IO.File]::AppendAllText($OutFile, 'wrong') } }
}
function Get-Item($LiteralPath) {
    if ($LiteralPath.EndsWith('ChatGPTLauncherSetup.exe')) { return [pscustomobject]@{VersionInfo=@{FileVersion=$calls.version}} }
    Microsoft.PowerShell.Management\Get-Item -LiteralPath $LiteralPath
}
function Start-Process($FilePath, $WorkingDirectory, $ArgumentList, $WindowStyle, [switch]$PassThru) {
    if ($FilePath -notlike '*.exe' -or $WindowStyle -ne 'Hidden' -or !$PassThru -or $ArgumentList[0] -ne '--headless') { throw 'Invalid native launch' }
    $calls.launch++; $calls.arguments = $ArgumentList
    $exit = if ($scenario -eq 'warning') { 10 } else { 0 }
    $native = [ordered]@{schemaVersion=1;operation='deploy';status=$(if ($exit) {'success-with-warnings'} else {'success'});
        exitCode=$exit;code='deployed';committed=$true;stage='post-actions';nativeError=0;warnings=@();actionsRequired=@();data=@{}}
    if ($scenario -eq 'mismatch') { $native.exitCode=50 }
    if ($scenario -ne 'crash') { [IO.File]::WriteAllText((Join-Path $WorkingDirectory 'native-result.json'), ($native | ConvertTo-Json -Depth 10)) }
    if ($scenario -eq 'keep') { [IO.File]::WriteAllText((Join-Path $WorkingDirectory 'unrelated.txt'), 'keep') }
    $process = [pscustomobject]@{ExitCode=$exit}
    $process | Add-Member ScriptMethod WaitForExit { }
    $process | Add-Member ScriptMethod Dispose { }
    return $process
}
$savedTemp = $env:TEMP
try {
    $env:TEMP = $testRoot
    $entry = Join-Path $ProjectRoot 'setup.ps1'
    foreach ($scenario in @('ok','warning','offline','checksum','old','crash','mismatch','keep')) {
        $calls.folder=$null; $calls.version = if ($scenario -eq 'old') { '1.0.6' } else { '1.0.7' }
        $before = $calls.launch
        $output = Join-Path $testRoot ($scenario + '.json')
        $values = @(& $entry -Headless -Request $requestFile -Result $output)
        if ($values.Count -ne 1) { throw "Not one result: $scenario" }
        $value = $values[0]
        $expected = switch ($scenario) {'ok' {0} 'warning' {10} 'keep' {10} 'crash' {72} 'mismatch' {72} default {71}}
        if ($value.exitCode -ne $expected) { throw "Wrong result ($($value.exitCode), expected $expected): $scenario" }
        $disk = [IO.File]::ReadAllText($output) | ConvertFrom-Json
        if ($disk.exitCode -ne $value.exitCode -or $disk.committed -ne $value.committed) { throw 'Result file mismatch' }
        $launched = $scenario -in @('ok','warning','crash','mismatch','keep')
        if (($calls.launch -gt $before) -ne $launched) { throw 'Old or invalid executable was launched' }
        if ($calls.folder) {
            if ($scenario -eq 'keep') {
                if (![IO.File]::Exists((Join-Path $calls.folder 'unrelated.txt')) -or [IO.File]::Exists((Join-Path $calls.folder 'ChatGPTLauncherSetup.exe'))) { throw 'Cleanup removed foreign content' }
            } elseif ([IO.Directory]::Exists($calls.folder)) { throw 'Temporary files remain' }
        }
    }
    $before = $calls.launch
    $existing = Join-Path $testRoot 'ok.json'; $prior = [IO.File]::ReadAllText($existing)
    $value = & $entry -Headless -Request $requestFile -Result $existing
    if ($value.exitCode -ne 70 -or $calls.launch -ne $before -or [IO.File]::ReadAllText($existing) -ne $prior) { throw 'Existing result was overwritten' }
    $value = & $entry -Headless -Request $requestFile -Result $requestFile
    if ($value.exitCode -ne 20 -or [IO.File]::ReadAllText($requestFile) -ne '{"schemaVersion":1}') { throw 'Request path was overwritten' }
    foreach ($name in @(('COM' + [char]0x00b9 + '.json'), ('LPT' + [char]0x00b2 + '.json'), ('COM' + [char]0x00b3 + '.json'))) {
        $value = & $entry -Headless -Request $requestFile -Result (Join-Path $testRoot $name)
        if ($value.exitCode -ne 20 -or $calls.launch -ne $before) { throw 'Reserved device path accepted' }
    }
    Write-Output 'PASS headless download: one structured result, caller session retained, no UI/progress, pinned assets, checksum/version gates, cleanup, native errors and output protection.'
} finally { $env:TEMP = $savedTemp }
