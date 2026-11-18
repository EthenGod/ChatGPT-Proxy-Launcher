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
$calls = @{launch=0; requests=0; folder=$null; version='1.1.0'; arguments=$null; nativeExit=$null}
function Write-Host { throw 'Headless wrote to the host' }
function Write-Progress { throw 'Headless showed progress' }
function Start-Sleep { throw 'Headless waited for completion text' }
function Invoke-RestMethod($Uri, $TimeoutSec) {
    $calls.requests++
    if ($ProgressPreference -ne 'SilentlyContinue') { throw 'Headless progress enabled' }
    if ($scenario -eq 'offline') { throw 'offline' }
    if ($scenario -eq 'beta') {
        if ($Uri -cnotlike '*/releases/tags/v1.2.0-beta.1') { throw 'Expected exact beta tag' }
        return @{tag_name='v1.2.0-beta.1'; draft=$false; prerelease=$true}
    }
    return @{tag_name='v1.1.0'; draft=$false; prerelease=$false}
}
function Invoke-WebRequest([switch]$UseBasicParsing, $Uri, $OutFile, $TimeoutSec) {
    $calls.folder = Split-Path $OutFile
    $expectedTag = if ($scenario -eq 'beta') { 'v1.2.0-beta.1' } else { 'v1.1.0' }
    if ($Uri -notlike "https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/$expectedTag/*") { throw 'Tag changed' }
    if ($OutFile.EndsWith('.txt')) { [IO.File]::WriteAllText($OutFile, "$hash  ChatGPTLauncherSetup.exe") }
    else { [IO.File]::WriteAllBytes($OutFile, $fixture); if ($scenario -eq 'checksum') { [IO.File]::AppendAllText($OutFile, 'wrong') } }
}
function Get-Item($LiteralPath) {
    if ($LiteralPath.EndsWith('ChatGPTLauncherSetup.exe')) {
        $fixed = [version]'0.0'
        if (![version]::TryParse(($calls.version -split '-')[0], [ref]$fixed)) { $fixed = [version]'0.0' }
        return [pscustomobject]@{VersionInfo=@{FileVersion=$calls.version; FileMajorPart=$fixed.Major; FileMinorPart=$fixed.Minor}}
    }
    Microsoft.PowerShell.Management\Get-Item -LiteralPath $LiteralPath
}
function Start-Process($FilePath, $WorkingDirectory, $ArgumentList, $WindowStyle, [switch]$PassThru) {
    if ($FilePath -notlike '*.exe' -or $WindowStyle -ne 'Hidden' -or !$PassThru -or $ArgumentList[0] -ne '--headless') { throw 'Invalid native launch' }
    $calls.launch++; $calls.arguments = $ArgumentList
    $exit = if ($null -ne $calls.nativeExit) { $calls.nativeExit } elseif ($scenario -eq 'warning') { 10 } else { 0 }
    $operation = if ($ArgumentList[1] -ceq 'companion') { 'companion-' + $ArgumentList[2] } else { $ArgumentList[1] }
    $committed = if ($exit -eq 41) { $null } elseif ($exit -gt 10) { $false } else { $operation -notin @('info','plan','companion-list') }
    $native = [ordered]@{schemaVersion=1;operation=$operation;status=$(if ($exit -gt 10) {'error'} elseif ($exit) {'success-with-warnings'} else {'success'});
        exitCode=$exit;code='fixture-result';committed=$committed;stage='post-actions';nativeError=0;warnings=@();actionsRequired=@();data=@{}}
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
    foreach ($scenario in @('ok','warning','offline','checksum','old','development','invalid-version','crash','mismatch','keep')) {
        $calls.folder=$null
        $calls.version = switch ($scenario) { 'old' { '1.0.6' } 'development' { '1.0.7' } 'invalid-version' { 'unknown' } default { '1.1.0' } }
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
    $scenario = 'beta'; $calls.version = '1.2.0-beta.1'
    $before = $calls.launch
    $value = & $entry -Headless -Operation info -Result (Join-Path $testRoot 'beta.json') -Channel Preview -Tag v1.2.0-beta.1
    if ($value.exitCode -ne 0 -or $value.transport.releaseTag -cne 'v1.2.0-beta.1' -or $calls.launch -ne $before+1) { throw 'Beta headless launch failed' }
    $value = & $entry -Headless -Operation info -Result (Join-Path $testRoot 'stable-only.json') -Tag v1.2.0-beta.1
    if ($value.exitCode -ne 71 -or $value.committed -ne $false -or $calls.launch -ne $before+1) { throw 'Stable channel launched a beta' }
    $before = $calls.requests
    foreach ($name in 'Branch','Tag') {
        $emptyFilter = @{$name=''}
        $value = & $entry -Headless -Operation info -Result (Join-Path $testRoot ("empty-$name.json")) @emptyFilter
        if ($value.exitCode -ne 71 -or $calls.requests -ne $before) { throw 'Empty filter fell back to a different release' }
    }
    # Each operation uses the native CLI spelling and preserves literal file paths.
    $scenario = 'ok'; $calls.version = '1.1.0'
    $manifestFile = Join-Path $testRoot ('manifest space & !% ' + [char]0x4e2d + '.json')
    [IO.File]::WriteAllText($manifestFile, '{"schemaVersion":1}')
    $operations = @(
        @{Operation='info'}, @{Operation='plan'; Request=$requestFile},
        @{Operation='companion-register'; Manifest=$manifestFile},
        @{Operation='companion-register'; Manifest=$manifestFile; Replace=$true},
        @{Operation='companion-configure'; Request=$requestFile},
        @{Operation='companion-remove'; Id='org.example.addon'}, @{Operation='companion-list'},
        @{Operation='INFO'}
    )
    foreach ($options in $operations) {
        $output = Join-Path $testRoot ([guid]::NewGuid().ToString('N') + '.json')
        $value = & $entry -Headless @options -Result $output
        $operation = $options.Operation.ToLowerInvariant()
        if ($value.exitCode -ne 0 -or $value.operation -cne $operation) { throw 'Operation was not forwarded' }
        $expectedArguments = @('--headless')
        if ($operation.StartsWith('companion-')) { $expectedArguments += @('companion', $operation.Substring(10)) }
        else { $expectedArguments += $operation }
        $expectedArguments += @('--result', ('"' + (Join-Path $calls.folder 'native-result.json') + '"'))
        if ($options.Request) { $expectedArguments += @('--request', ('"' + $options.Request + '"')) }
        if ($options.Manifest) { $expectedArguments += @('--manifest', ('"' + $options.Manifest + '"')) }
        if ($options.Id) { $expectedArguments += @('--id', $options.Id) }
        if ($options.Replace) { $expectedArguments += '--replace' }
        if (($calls.arguments -join '|') -cne ($expectedArguments -join '|')) { throw 'Native argument boundary changed' }
        if ([IO.Directory]::Exists($calls.folder) -or ![IO.File]::Exists($manifestFile)) { throw 'Extension input was removed or downloads remain' }
    }
    $invalidOptions = @(
        @{Operation='companion-register'}, @{Operation='companion-register'; Manifest=$manifestFile; Request=$requestFile},
        @{Operation='companion-configure'}, @{Operation='companion-configure'; Request=$requestFile; Replace=$true},
        @{Operation='companion-list'; Manifest=$manifestFile}, @{Operation='companion-list'; Replace=$true},
        @{Operation='companion-remove'}, @{Operation='companion-remove'; Id='org.example.addon'; Request=$requestFile},
        @{Operation='companion-remove'; Id='org.example.addon" --replace'}, @{Operation='companion-remove'; Id='org..example'},
        @{Operation='info'; Request=$requestFile}, @{Operation='deploy'; Request=$requestFile; Manifest=$manifestFile},
        @{Operation='companion-register'; Manifest=(Join-Path $testRoot 'absent.json')}
    )
    foreach ($options in $invalidOptions) {
        $beforeRequests = $calls.requests; $beforeLaunch = $calls.launch
        $value = & $entry -Headless @options -Result (Join-Path $testRoot ([guid]::NewGuid().ToString('N') + '.json'))
        if ($value.exitCode -ne 20 -or $calls.requests -ne $beforeRequests -or $calls.launch -ne $beforeLaunch) { throw 'Invalid operation downloaded or launched an EXE' }
    }
    $value = & $entry -Headless -Operation companion-register -Manifest $manifestFile -Result $manifestFile
    if ($value.exitCode -ne 20 -or [IO.File]::ReadAllText($manifestFile) -ne '{"schemaVersion":1}') { throw 'Manifest path was overwritten' }
    foreach ($nativeExit in @(10,20,21,30,31,40,41,50,70)) {
        $calls.nativeExit = $nativeExit
        $value = & $entry -Headless -Operation companion-register -Manifest $manifestFile -Result (Join-Path $testRoot ('native-' + $nativeExit + '.json'))
        if ($value.exitCode -ne $nativeExit -or $value.code -cne 'fixture-result' -or ($nativeExit -eq 41 -and $null -ne $value.committed)) { throw 'Native registry result was reclassified' }
    }
    $calls.nativeExit = $null; $calls.version = '1.0.6'; $before = $calls.launch
    $value = & $entry -Headless -Operation companion-list -Result (Join-Path $testRoot 'old-registry.json')
    if ($value.exitCode -ne 71 -or $calls.launch -ne $before) { throw 'Registry operation launched an old EXE' }
    $calls.version = '1.1.0'
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
    Write-Output 'PASS headless download: deploy/plan/info and all registry operations, argument validation/quoting, one result, no UI/progress, pinned assets, version gates, cleanup and output protection.'
} finally { $env:TEMP = $savedTemp }
