# SPDX-License-Identifier: GPL-3.0-only
param([string]$ProjectRoot = (Split-Path $PSScriptRoot), [switch]$SessionProbe)
$ErrorActionPreference = 'Stop'
$root = $ProjectRoot
$entry = Join-Path $root 'setup.ps1'
$oldTemp = $env:TEMP
$ProgressPreference = 'SilentlyContinue'
$testFolder = Join-Path $root ('work\download-tests-' + [guid]::NewGuid().ToString('N'))
$null = New-Item -ItemType Directory -Path $testFolder -Force
$calls = @{launches = 0; browserOpens = 0; waitMs = 0; messages = [Collections.Generic.List[string]]::new(); progressActive = $false; stages = [Collections.Generic.List[string]]::new()}
$fixture = [Text.Encoding]::ASCII.GetBytes('test setup payload')
$sha = [Security.Cryptography.SHA256]::Create()
try { $hash = [BitConverter]::ToString($sha.ComputeHash($fixture)).Replace('-', '').ToLowerInvariant() }
finally { $sha.Dispose() }
function Write-Host($Object, $ForegroundColor) { $calls.messages.Add([string]$Object) }
function Write-Progress($Id, $Activity, $Status, [switch]$Completed) {
    if ($Id -ne 1 -or !$Activity -or $ProgressPreference -ne 'Continue') { throw 'Invalid progress configuration' }
    $calls.progressActive = !$Completed
    if (!$Completed) { $calls.stages.Add($Status) }
}
function Start-Sleep($Milliseconds) {
    if ($Milliseconds -ne 100) { throw 'Unexpected polling interval' }
    if ($calls.folder -and [IO.Directory]::Exists($calls.folder)) { throw 'Completion shown before cleanup' }
    $calls.waitMs += $Milliseconds
    if ($SessionProbe) { Microsoft.PowerShell.Utility\Start-Sleep -Milliseconds $Milliseconds }
}
function Invoke-RestMethod($Uri, $TimeoutSec) {
    if (!$calls.progressActive -or $ProgressPreference -ne 'Continue') { throw 'No progress while fetching release' }
    if ($scenario -eq 'offline') { throw 'Offline' }
    if ($Uri -ne 'https://api.github.com/repos/EthenGod/ChatGPT-Proxy-Launcher/releases/latest') { throw 'Wrong API URL' }
    @{tag_name = 'v9.8.7'}
}
function Invoke-WebRequest([switch]$UseBasicParsing, $Uri, $OutFile, $TimeoutSec) {
    if (!$calls.progressActive -or $ProgressPreference -ne 'Continue') { throw 'Download progress disabled' }
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
    if ($calls.progressActive) { throw 'Progress left visible when launching a process' }
    if ($FilePath -eq 'https://github.com/EthenGod/ChatGPT-Proxy-Launcher') {
        if ([IO.Directory]::Exists($calls.folder) -or $calls.waitMs -ne 10000) { throw 'Browser opened before cleanup or timeout' }
        $calls.browserOpens++
        return
    }
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
    if ($SessionProbe) {
        $scenario = 'ok'
        Invoke-Expression ([IO.File]::ReadAllText($entry))
        throw 'The irm | iex session did not exit'
    }
    if (@([IO.File]::ReadAllBytes($entry) | Where-Object { $_ -gt 127 }).Count) { throw 'HTTP entry point must remain ASCII' }
    foreach ($scenario in 'ok','killed','offline','missing','duplicate','malformed','interrupted','corrupt','startfail','keep') {
        $success = $scenario -in 'ok','killed','keep'
        $calls.folder = $null
        $calls.waitMs = 0
        $calls.browserOpens = 0
        $calls.messages.Clear()
        $calls.stages.Clear()
        $before = $calls.launches
        $failed = $false
        try { & $entry } catch { if ($success) { throw }; $failed = $true }
        if ($calls.progressActive -or !$calls.stages.Count) { throw "Progress not shown or cleared: $scenario" }
        if ($ProgressPreference -ne 'SilentlyContinue') { throw 'Caller progress preference changed' }
        if ($success -and $calls.stages.Count -ne 4) { throw "Missing progress stage: $scenario" }
        if ($success) {
            if ($failed -or $calls.launches -ne $before + 1) { throw 'Valid download did not launch once' }
            if (![IO.File]::Exists((Join-Path $testFolder "$scenario.observed"))) { throw "Did not wait for child: $scenario" }
        } elseif (!$failed -or $calls.launches -ne $before) { throw "Failed to stop: $scenario" }
        if ($scenario -eq 'keep') {
            if (![IO.File]::Exists((Join-Path $calls.folder 'unrelated.txt')) -or
                [IO.File]::Exists((Join-Path $calls.folder 'ChatGPTLauncherSetup.exe')) -or
                [IO.File]::Exists((Join-Path $calls.folder 'SHA256SUMS.txt'))) { throw 'Incorrect cleanup in nonempty folder' }
        } elseif ($calls.folder -and [IO.Directory]::Exists($calls.folder)) { throw "Download folder remains: $scenario" }
        $completed = $scenario -in 'ok','killed'
        if ($calls.waitMs -ne $(if ($completed) { 10000 } else { 0 })) { throw "Wrong exit delay: $scenario" }
        if ($calls.browserOpens -ne $(if ($completed) { 1 } else { 0 })) { throw "Wrong browser launch count: $scenario" }
        $star = [regex]::Unescape('\u559c\u6b22\u672c\u9879\u76ee\u8bf7\u7ed9\u672c\u9879\u76ee\u70b9\u4e00\u4e2a Star \u8c22\u8c22\u55b5~')
        if ($calls.messages.Contains($star) -ne $completed) { throw "Wrong completion message: $scenario" }
    }
    # Exercise immediate keys (including non-text keys), delayed input and timeout without opening a browser.
    $tokens = $null; $parseErrors = $null
    $ast = [Management.Automation.Language.Parser]::ParseFile($entry, [ref]$tokens, [ref]$parseErrors)
    if ($parseErrors.Count) { throw 'Entry point parse error' }
    $footer = $ast.Find({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Show-SetupCompletion' }, $true)
    $keyCases = @(
        @{ key = 'X'; waitTicks = 0 },
        @{ key = ' '; waitTicks = 0 },
        @{ key = "`r"; waitTicks = 0 },
        @{ key = [ConsoleKeyInfo]::new([char]0, [ConsoleKey]::LeftArrow, $false, $false, $false); waitTicks = 3 },
        @{ key = $null; waitTicks = 100 }
    )
    foreach ($browserFails in $false,$true) {
        foreach ($keyCase in $keyCases) { & {
            Invoke-Expression $footer.Extent.Text
            $polls = @{ count = 0 }
            $urls = [Collections.Generic.List[string]]::new()
            $calls.folder = $null; $calls.waitMs = 0
            function Read-SetupKey {
                $polls.count++
                if ($polls.count -gt $keyCase.waitTicks) { return $keyCase.key }
                return $null
            }
            function Start-Process($FilePath) {
                $urls.Add($FilePath)
                if ($browserFails) { throw 'Simulated browser failure' }
            }
            Show-SetupCompletion
            if ($urls.Count -ne 1 -or $urls[0] -ne 'https://github.com/EthenGod/ChatGPT-Proxy-Launcher' -or
                $calls.waitMs -ne (100 * $keyCase.waitTicks)) { throw 'Incorrect any-key or timeout behavior' }
        } }
    }
    # A separate real PowerShell session runs the same mocked downloads via iex, waits ten seconds and exits.
    $testPath64 = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($PSCommandPath))
    $root64 = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($root))
    $probe = @'
$ErrorActionPreference = 'Stop'
$testPath = [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String('TEST_PATH'))
$projectRoot = [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String('PROJECT_ROOT'))
& ([scriptblock]::Create([IO.File]::ReadAllText($testPath))) -ProjectRoot $projectRoot -SessionProbe
exit 71
'@
    $encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($probe.Replace('TEST_PATH', $testPath64).Replace('PROJECT_ROOT', $root64)))
    $timer = [Diagnostics.Stopwatch]::StartNew()
    $child = Microsoft.PowerShell.Management\Start-Process -FilePath ([Diagnostics.Process]::GetCurrentProcess().MainModule.FileName) -ArgumentList @('-NoProfile','-NonInteractive','-EncodedCommand',$encoded) -WindowStyle Hidden -PassThru
    try {
        if (!$child.WaitForExit(30000)) { $child.Kill(); throw 'Session exit timed out' }
        if ($child.ExitCode -ne 0 -or $timer.Elapsed.TotalSeconds -lt 10) { throw 'Session did not wait and exit correctly' }
    } finally { $child.Dispose() }
    Write-Output 'PASS: download progress and preference scope, checks, cleanup, completion text, any-key/timeout browser launch and session exit (isolated processes; browser mocked).'
} finally { $env:TEMP = $oldTemp }
