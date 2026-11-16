# SPDX-License-Identifier: GPL-3.0-only
param(
    [string]$ProjectRoot = (Split-Path $PSScriptRoot),
    [switch]$SkipGui,
    [ValidateRange(1, 120)][int]$IdleSeconds = 60
)
$ErrorActionPreference = 'Stop'
$ProjectRoot = [IO.Path]::GetFullPath($ProjectRoot)
$testRoot = Join-Path $ProjectRoot ('work\ui-contract-' + [guid]::NewGuid().ToString('N'))
$null = [IO.Directory]::CreateDirectory($testRoot)
$native = Join-Path $ProjectRoot 'work\setup-bridge-runner.exe'
$desktop = Join-Path $ProjectRoot 'work\ui-stage1-test\ChatGPTLauncherSetup.exe'
$production = Join-Path $ProjectRoot 'work\ui-stage1\ChatGPTLauncherSetup.exe'
foreach ($file in @($native, $desktop, $production)) {
    if (![IO.File]::Exists($file)) { throw "Build the bridge runner, normal UI and test harness first: $file" }
}
function Require([bool]$Value, [string]$Message) { if (!$Value) { throw $Message } }
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class SetupPeResource {
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    static extern IntPtr LoadLibraryExW(string path, IntPtr file, uint flags);
    [DllImport("kernel32.dll", SetLastError=true)]
    static extern IntPtr FindResourceW(IntPtr module, IntPtr name, IntPtr type);
    [DllImport("kernel32.dll")] static extern uint SizeofResource(IntPtr module, IntPtr resource);
    [DllImport("kernel32.dll")] static extern IntPtr LoadResource(IntPtr module, IntPtr resource);
    [DllImport("kernel32.dll")] static extern IntPtr LockResource(IntPtr resource);
    [DllImport("kernel32.dll")] static extern bool FreeLibrary(IntPtr module);
    public static byte[] Read(string path, int name, int type) {
        // Map data only. Never run the executable or its imported code.
        IntPtr module = LoadLibraryExW(path, IntPtr.Zero, 0x22);
        if (module == IntPtr.Zero) throw new InvalidOperationException("Cannot map PE resources");
        try {
            IntPtr resource = FindResourceW(module, new IntPtr(name), new IntPtr(type));
            uint size = SizeofResource(module, resource);
            if (resource == IntPtr.Zero || size == 0 || size > 32 * 1024 * 1024) throw new InvalidOperationException("Invalid PE resource");
            IntPtr data = LockResource(LoadResource(module, resource));
            if (data == IntPtr.Zero) throw new InvalidOperationException("Cannot read PE resource");
            byte[] bytes = new byte[size];
            Marshal.Copy(data, bytes, 0, bytes.Length);
            return bytes;
        } finally { FreeLibrary(module); }
    }
}
'@
$expectedManifest = [xml][IO.File]::ReadAllText((Join-Path $ProjectRoot 'src\Setup.manifest'))
foreach ($exe in @($production, $desktop)) {
    $metadata = Get-Content -LiteralPath ((Split-Path $exe) + '-build.json') -Raw | ConvertFrom-Json
    Require ((Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash -eq $metadata.setupSha256) 'EXE changed after its recorded build'
    $bytes = [SetupPeResource]::Read($exe, 101, 10)
    $hash = [Security.Cryptography.SHA256]::Create()
    try { $payloadHash = [BitConverter]::ToString($hash.ComputeHash($bytes)).Replace('-', '') }
    finally { $hash.Dispose() }
    Require ($payloadHash -eq $metadata.launcherSha256 -and $bytes.Length -eq $metadata.payloadBytes) 'Embedded launcher differs from its own build'
    # Tauri's resource compiler flattens XML whitespace and adds outer spaces.
    $manifest = [xml][Text.Encoding]::UTF8.GetString([SetupPeResource]::Read($exe, 1, 24)).TrimStart([char]0xFEFF).Trim()
    Require ($manifest.OuterXml -ceq $expectedManifest.OuterXml) 'Desktop manifest differs from the native manifest'
}
Write-Host 'PASS single EXE resources: exact same-build launcher payload and original Windows manifest'
function Quote-Argument([string]$Value) {
    # Windows argv quoting, including empty values, quotes and trailing backslashes.
    $escaped = [regex]::Replace($Value, '(\\*)"', '$1$1\"')
    $escaped = [regex]::Replace($escaped, '(\\+)$', '$1$1')
    return '"' + $escaped + '"'
}
function Start-Isolated([string]$Exe, [string]$Root, [string]$Tail, [bool]$WithoutWebView = $false) {
    $info = New-Object Diagnostics.ProcessStartInfo
    $info.FileName = $Exe
    $info.Arguments = $Tail
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.WindowStyle = 'Hidden'
    $info.WorkingDirectory = $Root
    $info.EnvironmentVariables['CHATGPT_SETUP_TEST_ROOT'] = $Root
    if ($WithoutWebView) { $info.EnvironmentVariables['WEBVIEW2_BROWSER_EXECUTABLE_FOLDER'] = (Join-Path $Root 'no-webview') }
    return [Diagnostics.Process]::Start($info)
}
function Run-Cli([string]$Exe, [string]$Root, [string]$Tail) {
    $timer = [Diagnostics.Stopwatch]::StartNew()
    $process = Start-Isolated $Exe $Root $Tail $true
    try {
        Require ($process.WaitForExit(20000)) 'Headless process did not exit; no forced termination was attempted.'
        return [pscustomobject]@{exitCode=$process.ExitCode; elapsedMs=$timer.Elapsed.TotalMilliseconds}
    } finally { $process.Dispose() }
}
$timings = @()
$caseIndex = 0
function Compare-Cli([string]$Root, [string]$Tail, [int]$Expected) {
    $script:caseIndex++
    $results = @()
    foreach ($exe in @($native, $desktop)) {
        $output = Join-Path $Root ("result-$script:caseIndex-$($results.Count).json")
        $operation = [regex]::Match($Tail, '^--headless (?:companion \S+|\S+)')
        $run = Run-Cli $exe $Root ($Tail.Insert($operation.Length, ' --result ' + (Quote-Argument $output)))
        Require ($run.exitCode -eq $Expected) "CLI case $script:caseIndex: expected $Expected, received $($run.exitCode)"
        Require ([IO.File]::Exists($output)) "CLI case $script:caseIndex did not write its result"
        $results += [IO.File]::ReadAllText($output)
        $script:timings += [pscustomobject]@{case=$script:caseIndex; implementation=$(if ($exe -eq $native) {'native'} else {'tauri'}); elapsedMs=[Math]::Round($run.elapsedMs, 2)}
    }
    Require ($results[0] -ceq $results[1]) "Native/Tauri result mismatch in case $script:caseIndex"
    Require (![IO.File]::Exists((Join-Path $Root 'gui-entered.json'))) 'Headless path entered the GUI bootstrap'
    return ($results[1] | ConvertFrom-Json)
}
$cliRoot = Join-Path $testRoot 'cli space 中文'
$null = [IO.Directory]::CreateDirectory($cliRoot)
$null = Compare-Cli $cliRoot '--headless info' 0
$extra = @('', '中文 参数', 'quote"end', 'C:\ending\', '& % !')
$tail = '--headless plan --base defaults --startup off --desktop off --codex-env leave --official-startup leave --proxy socks5://127.0.0.1:7890'
foreach ($argument in $extra) { $tail += ' --extra-arg ' + (Quote-Argument $argument) }
$plan = Compare-Cli $cliRoot $tail 0
Require (!$plan.committed) 'Plan committed files'
Require (![IO.Directory]::Exists((Join-Path $cliRoot 'state'))) 'Plan created deployment files'
$null = Compare-Cli $cliRoot ($tail.Replace('--headless plan', '--headless deploy')) 0
$null = Compare-Cli $cliRoot '--headless info' 0
$null = Compare-Cli $cliRoot '--headless companion list' 0
$null = Compare-Cli $cliRoot '--headless invalid' 20
$null = Compare-Cli $cliRoot '--headless plan --startup invalid' 20
$null = Compare-Cli $cliRoot ('--headless plan --extra-arg ' + (Quote-Argument ('x' * 16000))) 20
foreach ($name in @('startup.lnk', 'desktop.lnk', 'legacy-startup.lnk', 'legacy-desktop.lnk', 'codex.env')) {
    Require (![IO.File]::Exists((Join-Path $cliRoot $name))) "Unexpected system-operation fixture: $name"
}
$reserved = Join-Path $cliRoot 'reserved.json'
[IO.File]::WriteAllText($reserved, 'keep')
foreach ($exe in @($native, $desktop)) {
    $run = Run-Cli $exe $cliRoot ('--headless info --result ' + (Quote-Argument $reserved))
    Require ($run.exitCode -eq 70) 'Existing result was not rejected'
    Require ([IO.File]::ReadAllText($reserved) -ceq 'keep') 'Existing result was overwritten'
}
$run = Run-Cli $production $cliRoot '--invalid'
Require ($run.exitCode -eq 20) 'Normal EXE failed the no-WebView headless check'
Write-Host "PASS desktop CLI: $caseIndex exact native/Tauri result comparisons, original arguments, create-new results, no GUI bootstrap"

function Get-ProcessTree([int]$RootId) {
    $rows = @(Get-CimInstance Win32_Process -Property ProcessId,ParentProcessId,Name)
    $ids = New-Object 'Collections.Generic.HashSet[int]'
    $null = $ids.Add($RootId)
    do {
        $added = $false
        foreach ($row in $rows) { if ($ids.Contains([int]$row.ParentProcessId) -and $ids.Add([int]$row.ProcessId)) { $added = $true } }
    } while ($added)
    return @($rows | Where-Object { $ids.Contains([int]$_.ProcessId) })
}
$gui = @()
if (!$SkipGui) {
    for ($cycle = 1; $cycle -le 3; $cycle++) {
        $root = Join-Path $testRoot "window-$cycle"
        $null = [IO.Directory]::CreateDirectory($root)
        $timer = [Diagnostics.Stopwatch]::StartNew()
        $process = Start-Isolated $desktop $root ''
        try {
            $deadline = [DateTime]::UtcNow.AddSeconds(30)
            while (![IO.File]::Exists((Join-Path $root 'ui-ready.json'))) {
                Require (!$process.HasExited) 'Desktop exited before the page became ready'
                Require ([DateTime]::UtcNow -lt $deadline) 'Desktop did not become ready; inspect its test window'
                Start-Sleep -Milliseconds 100
            }
            $readyMs = $timer.Elapsed.TotalMilliseconds
            $before = Get-Content -LiteralPath (Join-Path $root 'read-ok.json') -Raw | ConvertFrom-Json
            Require ($before.reads -eq 1 -and $before.payloadBytes -gt 0) 'Native state did not reach the page exactly once'
            $seconds = if ($cycle -eq 1) { $IdleSeconds } else { 2 }
            Write-Host "Desktop cycle $cycle ready; observing idle for $seconds seconds"
            for ($second = 0; $second -lt $seconds; $second++) { Start-Sleep -Seconds 1 }
            $after = Get-Content -LiteralPath (Join-Path $root 'read-ok.json') -Raw | ConvertFrom-Json
            Require ($after.reads -eq $before.reads) 'Idle page repeated its bridge read'
            $tree = @(Get-ProcessTree $process.Id)
            $handles = @($tree | ForEach-Object { Get-Process -Id $_.ProcessId -ErrorAction SilentlyContinue })
            $workingSet = ($handles | Measure-Object WorkingSet64 -Sum).Sum
            $private = ($handles | Measure-Object PrivateMemorySize64 -Sum).Sum
            $gui += [pscustomobject]@{cycle=$cycle; readyMs=[Math]::Round($readyMs, 2); idleSeconds=$seconds; processes=$tree.Count; workingSetMiB=[Math]::Round($workingSet / 1MB, 2); privateMiB=[Math]::Round($private / 1MB, 2); reads=$after.reads}
            $process.Refresh()
            Require ($process.CloseMainWindow()) 'Could not request a normal window close'
            Require ($process.WaitForExit(10000)) 'Desktop did not exit after normal close'
            $deadline = [DateTime]::UtcNow.AddSeconds(10)
            do {
                $remaining = @($handles | Where-Object { !$_.HasExited })
                if ($remaining.Count) { Start-Sleep -Milliseconds 100 }
            } while ($remaining.Count -and [DateTime]::UtcNow -lt $deadline)
            Require ($remaining.Count -eq 0) 'A process from this UI tree remained after close'
            foreach ($handle in $handles) { $handle.Dispose() }
            Require (![IO.Directory]::Exists((Join-Path $root 'state'))) 'Read-only GUI wrote deployment state'
        } finally { $process.Dispose() }
    }
    Write-Host 'PASS desktop lifecycle: embedded page, real native command, idle without bridge polling, three normal closes with no remaining owned process'
}
$report = [ordered]@{
    schemaVersion=1
    sizes=[ordered]@{nativeBytes=(Get-Item (Join-Path $ProjectRoot 'outputs\ChatGPTLauncherSetup.exe')).Length; tauriBytes=(Get-Item $production).Length; launcherBytes=(Get-Item (Join-Path $ProjectRoot 'outputs\ChatGPTProxyLauncher.exe')).Length}
    cli=$timings
    gui=$gui
}
[IO.File]::WriteAllText((Join-Path $testRoot 'summary.json'), ($report | ConvertTo-Json -Depth 10))
Write-Host "Local measurements: $testRoot\summary.json"
