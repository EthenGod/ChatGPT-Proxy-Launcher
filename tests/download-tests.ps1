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
    $calls.folder = Split-Path $OutFile
    $asset = Split-Path $OutFile -Leaf
    if ($Uri -ne "https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v9.8.7/$asset") { throw 'Wrong asset URL' }
    if ($asset -eq 'SHA256SUMS.txt') {
        $line = "$hash  ChatGPTLauncherSetup.exe"
        if ($scenario -eq 'missing') { $line = "$hash  Other.exe" }
        if ($scenario -eq 'duplicate') { $line += [Environment]::NewLine + $line }
        if ($scenario -eq 'malformed') { $line = 'abcd  ChatGPTLauncherSetup.exe' }
        [IO.File]::WriteAllText($OutFile, $line)
    } elseif ($asset -eq 'ChatGPTLauncherSetup.exe') {
        [IO.File]::WriteAllBytes($OutFile, $fixture)
        if ($scenario -eq 'interrupted') { throw 'Interrupted download' }
        if ($scenario -eq 'corrupt') { [IO.File]::AppendAllText($OutFile, 'x') }
    } else { throw 'Unexpected asset' }
}
function Start-Process($FilePath, $WorkingDirectory, [switch]$PassThru) {
    if (!(Test-Path -LiteralPath $FilePath) -or (Split-Path $FilePath) -ne $WorkingDirectory -or
        (Split-Path $WorkingDirectory) -ne $testFolder) { throw 'Unexpected launch path' }
    if (!$PassThru) { throw 'No process handle requested' }
    if ($scenario -eq 'startfail') { throw 'Simulated launch failure' }
    if ($scenario -eq 'keep') { [IO.File]::WriteAllText((Join-Path $WorkingDirectory 'unrelated.txt'), 'keep') }
    # A real child inspects the downloads before exiting normally or terminating itself.
    $probe = @'
Start-Sleep -Milliseconds 150
if (![IO.File]::Exists('ChatGPTLauncherSetup.exe') -or ![IO.File]::Exists('SHA256SUMS.txt')) { exit 73 }
[IO.File]::WriteAllText('..\SCENARIO.observed', 'ready')
if ('SCENARIO' -eq 'killed') { [Diagnostics.Process]::GetCurrentProcess().Kill() }
'@
    $encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($probe.Replace('SCENARIO', $scenario)))
    $calls.launches++
    Microsoft.PowerShell.Management\Start-Process -FilePath ([Diagnostics.Process]::GetCurrentProcess().MainModule.FileName) -ArgumentList @('-NoProfile','-NonInteractive','-EncodedCommand',$encoded) -WorkingDirectory $WorkingDirectory -WindowStyle Hidden -PassThru
}
try {
    $env:TEMP = $testFolder
    foreach ($scenario in 'ok','killed','offline','missing','duplicate','malformed','interrupted','corrupt','startfail','keep') {
        $success = $scenario -in 'ok','killed','keep'
        $calls.folder = $null
        $before = $calls.launches
        $failed = $false
        try { & $entry } catch { if ($success) { throw }; $failed = $true }
        if ($success) {
            if ($failed -or $calls.launches -ne $before + 1) { throw 'Valid download did not launch once' }
            if (![IO.File]::Exists((Join-Path $testFolder "$scenario.observed"))) { throw "Did not wait for child: $scenario" }
        } elseif (!$failed -or $calls.launches -ne $before) { throw "Failed to stop: $scenario" }
        if ($scenario -eq 'keep') {
            if (![IO.File]::Exists((Join-Path $calls.folder 'unrelated.txt')) -or
                [IO.File]::Exists((Join-Path $calls.folder 'ChatGPTLauncherSetup.exe')) -or
                [IO.File]::Exists((Join-Path $calls.folder 'SHA256SUMS.txt'))) { throw 'Incorrect cleanup in nonempty folder' }
        } elseif ($calls.folder -and [IO.Directory]::Exists($calls.folder)) { throw "Download folder remains: $scenario" }
    }
    Write-Output 'PASS: download validation and cleanup after normal exit, forced exit and failures; unrelated files retained (isolated child processes).'
} finally { $env:TEMP = $oldTemp }
