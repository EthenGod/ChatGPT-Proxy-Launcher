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
$production = Join-Path $ProjectRoot 'outputs\ChatGPTLauncherSetup.exe'
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
public static class SetupWindowClose {
    [DllImport("user32.dll", SetLastError=true)]
    static extern bool PostMessageW(IntPtr window, uint message, UIntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")]
    static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);
    [DllImport("user32.dll")]
    static extern IntPtr GetClassLongPtrW(IntPtr window, int index);
    [DllImport("user32.dll")]
    public static extern bool IsIconic(IntPtr window);
    [DllImport("user32.dll")]
    public static extern bool ShowWindowAsync(IntPtr window, int command);
    public static bool HasClassIcons(IntPtr window) {
        return GetClassLongPtrW(window, -14) != IntPtr.Zero && GetClassLongPtrW(window, -34) != IntPtr.Zero;
    }
    public static void Request(IntPtr window, int expectedProcessId) {
        uint owner;
        if (window == IntPtr.Zero || GetWindowThreadProcessId(window, out owner) == 0 || owner != expectedProcessId)
            throw new InvalidOperationException("Unexpected setup window owner");
        if (!PostMessageW(window, 0x0112, new UIntPtr(0xF060), IntPtr.Zero))
            throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
    }
}
'@
$expectedManifest = [xml][IO.File]::ReadAllText((Join-Path $ProjectRoot 'src\Setup.manifest'))
$launcher = Join-Path $ProjectRoot 'outputs\ChatGPTProxyLauncher.exe'
$expectedIcon = [SetupPeResource]::Read($launcher, 1, 14)
$iconCount = [BitConverter]::ToUInt16($expectedIcon, 4)
foreach ($exe in @($production, $desktop)) {
    $metadataPath = if ($exe -eq $production) { Join-Path $ProjectRoot 'work\ui-build.json' } else { (Split-Path $exe) + '-build.json' }
    $metadata = Get-Content -LiteralPath $metadataPath -Raw | ConvertFrom-Json
    Require ((Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash -eq $metadata.setupSha256) 'EXE changed after its recorded build'
    $bytes = [SetupPeResource]::Read($exe, 101, 10)
    $hash = [Security.Cryptography.SHA256]::Create()
    try { $payloadHash = [BitConverter]::ToString($hash.ComputeHash($bytes)).Replace('-', '') }
    finally { $hash.Dispose() }
    Require ($payloadHash -eq $metadata.launcherSha256 -and $bytes.Length -eq $metadata.payloadBytes) 'Embedded launcher differs from its own build'
    # Tauri's resource compiler flattens XML whitespace and adds outer spaces.
    $manifest = [xml][Text.Encoding]::UTF8.GetString([SetupPeResource]::Read($exe, 1, 24)).TrimStart([char]0xFEFF).Trim()
    Require ($manifest.OuterXml -ceq $expectedManifest.OuterXml) 'Desktop manifest differs from the native manifest'
    $capability = [Text.Encoding]::UTF8.GetString([SetupPeResource]::Read($exe, 102, 10)) | ConvertFrom-Json
    Require ($capability.schemaVersion -eq 1 -and $capability.requiresWebView2 -eq $true -and $capability.compatibilityArgument -ceq '--compatibility') 'Missing dual-UI capability resource'
    $icon = [SetupPeResource]::Read($exe, 32512, 14)
    Require ([BitConverter]::ToUInt16($icon, 4) -eq $iconCount -and $iconCount -gt 0) 'Desktop icon sizes differ from the launcher'
    for ($index=0; $index -lt $iconCount; $index++) {
        $offset=6+14*$index+12
        $expectedImage=[SetupPeResource]::Read($launcher, [BitConverter]::ToUInt16($expectedIcon, $offset), 3)
        $image=[SetupPeResource]::Read($exe, [BitConverter]::ToUInt16($icon, $offset), 3)
        Require ([Convert]::ToBase64String($image) -ceq [Convert]::ToBase64String($expectedImage)) 'Desktop icon image differs from the launcher'
    }
}
Write-Host 'PASS single EXE resources: exact same-build launcher payload, shared icon images and original Windows manifest'
function Quote-Argument([string]$Value) {
    # Windows argv quoting, including empty values, quotes and trailing backslashes.
    $escaped = [regex]::Replace($Value, '(\\*)"', '$1$1\"')
    $escaped = [regex]::Replace($escaped, '(\\+)$', '$1$1')
    return '"' + $escaped + '"'
}
function Start-Isolated([string]$Exe, [string]$Root, [string]$Tail, [bool]$WithoutWebView = $false,
    [bool]$ShowWindow = $false, [bool]$WithoutPageScript = $false) {
    $info = New-Object Diagnostics.ProcessStartInfo
    $info.FileName = $Exe
    $info.Arguments = $Tail
    $info.UseShellExecute = $false
    $info.CreateNoWindow = !$ShowWindow
    $info.WindowStyle = if ($ShowWindow) { 'Normal' } else { 'Hidden' }
    $info.WorkingDirectory = $Root
    $info.EnvironmentVariables['CHATGPT_SETUP_TEST_ROOT'] = $Root
    if ($WithoutWebView) { $info.EnvironmentVariables['WEBVIEW2_BROWSER_EXECUTABLE_FOLDER'] = (Join-Path $Root 'no-webview') }
    if ($WithoutPageScript) { $info.EnvironmentVariables['WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS'] = '--blink-settings=scriptEnabled=false' }
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
$rootPlan = Compare-Cli $cliRoot ('--headless plan --directory ' + (Quote-Argument $cliRoot)) 0
Require ($rootPlan.data.directory -eq $cliRoot) 'A launcher directly inside the isolated root must be allowed'
$null = Compare-Cli $cliRoot ($tail.Replace('--headless plan', '--headless deploy')) 0
$null = Compare-Cli $cliRoot '--headless info' 0
$null = Compare-Cli $cliRoot '--headless companion list' 0
$sibling = $cliRoot + '-sibling'
$requestPath = Join-Path $cliRoot 'outside-request.json'
[IO.File]::WriteAllText($requestPath, (@{schemaVersion=1;settings=@{directory=$sibling}} | ConvertTo-Json))
$rejected = Compare-Cli $cliRoot ('--headless deploy --request ' + (Quote-Argument $requestPath)) 20
Require ($rejected.code -eq 'outside-test-root' -and !$rejected.committed) 'Isolated deployment accepted a sibling directory'
Require (![IO.Directory]::Exists($sibling)) 'Isolated deployment wrote outside its root'
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
    $outsideResult = Join-Path $testRoot 'outside-result.json'
    foreach ($suffix in @('', ' --startup invalid')) {
        $run = Run-Cli $exe $cliRoot ('--headless plan --result ' + (Quote-Argument $outsideResult) + $suffix)
        Require ($run.exitCode -eq 70 -and ![IO.File]::Exists($outsideResult)) 'An isolated result escaped its root'
    }
}
$run = Run-Cli $production $cliRoot '--invalid'
Require ($run.exitCode -eq 20) 'Normal EXE failed the no-WebView headless check'
foreach ($tail in @('--compatibility --headless info', '--headless info --compatibility', '--compatibility ""', '--compatibilityx')) {
    $run = Run-Cli $desktop $cliRoot $tail
    Require ($run.exitCode -eq 20) 'Combined or invalid compatibility argument must not open a GUI'
}
Require (![IO.File]::Exists((Join-Path $cliRoot 'compatibility-entered.json'))) 'CLI entered compatibility GUI'
Write-Host "PASS desktop CLI: $caseIndex exact native/Tauri result comparisons, original arguments, create-new results, no GUI bootstrap"

function Get-ProcessTree([int]$RootId) {
    $rows = @(Get-CimInstance Win32_Process -Property ProcessId,ParentProcessId,Name,CreationDate)
    $byId = @{}
    foreach ($row in $rows) { $byId[[int]$row.ProcessId] = $row }
    $ids = New-Object 'Collections.Generic.HashSet[int]'
    $null = $ids.Add($RootId)
    do {
        $added = $false
        foreach ($row in $rows) {
            $parent = $byId[[int]$row.ParentProcessId]
            if ($parent -and $row.CreationDate -ge $parent.CreationDate -and
                $ids.Contains([int]$row.ParentProcessId) -and $ids.Add([int]$row.ProcessId)) { $added = $true }
        }
    } while ($added)
    return @($rows | Where-Object { $ids.Contains([int]$_.ProcessId) })
}
$gui = @()
$compatibility = @()
function Wait-Visible([Diagnostics.Process]$Process) {
    $deadline = [DateTime]::UtcNow.AddSeconds(20)
    do {
        $Process.Refresh()
        Require (!$Process.HasExited -and [DateTime]::UtcNow -lt $deadline) 'Isolated window did not become visible'
        if ($Process.MainWindowHandle -eq [IntPtr]::Zero) { Start-Sleep -Milliseconds 100 }
    } while ($Process.MainWindowHandle -eq [IntPtr]::Zero)
}
function Check-Duplicate([string]$Root, [string]$Tail, [IntPtr]$FirstWindow) {
    $null = [SetupWindowClose]::ShowWindowAsync($FirstWindow, 6)
    $deadline = [DateTime]::UtcNow.AddSeconds(3)
    while (![SetupWindowClose]::IsIconic($FirstWindow) -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 50 }
    Require ([SetupWindowClose]::IsIconic($FirstWindow)) 'Could not minimize the isolated test window'
    $duplicate = Start-Isolated $desktop $Root $Tail
    try {
        Require ($duplicate.WaitForExit(10000) -and $duplicate.ExitCode -eq 0) 'A second GUI did not defer to the existing window'
        $deadline = [DateTime]::UtcNow.AddSeconds(3)
        while ([SetupWindowClose]::IsIconic($FirstWindow) -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 50 }
        Require (![SetupWindowClose]::IsIconic($FirstWindow)) 'Duplicate launch did not restore the original window'
    } finally {
        if (!$duplicate.HasExited) { $null = $duplicate.CloseMainWindow() }
        $duplicate.Dispose()
    }
}
if (!$SkipGui) {
    $root = Join-Path $testRoot 'no-page-script'
    $null = [IO.Directory]::CreateDirectory($root)
    $process = Start-Isolated $desktop $root '' $false $true $true
    try {
        Wait-Visible $process
        Require ([IO.File]::Exists((Join-Path $root 'gui-entered.json'))) 'Expected a WebView window'
        [SetupWindowClose]::Request($process.MainWindowHandle, $process.Id)
        Require ($process.WaitForExit(10000) -and $process.ExitCode -eq 0) 'Window could not close before the page handshake'
        Require (![IO.File]::Exists((Join-Path $root 'ui-ready.json'))) 'Expected the page handshake to be absent'
    } finally {
        if (!$process.HasExited) { $null = $process.CloseMainWindow() }
        $process.Dispose()
    }
    Write-Host 'PASS window bootstrap: visible and closable with the page script disabled'
    foreach ($case in @(@('--compatibility', $false), @('"--compatibility" ', $true), @('', $true))) {
        $root = Join-Path $testRoot ('compatibility-' + $compatibility.Count)
        $null = [IO.Directory]::CreateDirectory($root)
        $timer = [Diagnostics.Stopwatch]::StartNew()
        $process = Start-Isolated $desktop $root $case[0] $case[1] $true
        try {
            Wait-Visible $process
            $readyMs = $timer.Elapsed.TotalMilliseconds
            Require ([SetupWindowClose]::HasClassIcons($process.MainWindowHandle)) 'Compatibility window is missing its large or small icon'
            if ($compatibility.Count -eq 0) { Check-Duplicate $root '' $process.MainWindowHandle }
            Start-Sleep -Seconds 2
            Require ([IO.File]::Exists((Join-Path $root 'compatibility-entered.json'))) 'Native compatibility entry was not used'
            Require (![IO.File]::Exists((Join-Path $root 'gui-entered.json'))) 'Compatibility constructed Tauri state'
            Require (![IO.Directory]::Exists((Join-Path $root 'webview'))) 'Compatibility created WebView data'
            $tree = @(Get-ProcessTree $process.Id)
            Require ($tree.Count -eq 1) 'Compatibility created a child process'
            $process.Refresh()
            $compatibility += [pscustomobject]@{argument=$case[0]; withoutWebView=$case[1]; readyMs=[Math]::Round($readyMs,2); processes=$tree.Count;
                privateMiB=[Math]::Round($process.PrivateMemorySize64 / 1MB,2); workingSetMiB=[Math]::Round($process.WorkingSet64 / 1MB,2)}
            [SetupWindowClose]::Request($process.MainWindowHandle, $process.Id)
            Require ($process.WaitForExit(10000) -and $process.ExitCode -eq 0) 'Compatibility did not exit cleanly'
            Require (![IO.Directory]::Exists((Join-Path $root 'state'))) 'Compatibility preview wrote deployment data'
        } finally {
            if (!$process.HasExited) { $null = $process.CloseMainWindow() }
            $process.Dispose()
        }
    }
    Write-Host 'PASS compatibility: explicit/quoted switch and missing-runtime fallback, single process, no WebView, three normal closes'
    for ($cycle = 1; $cycle -le 3; $cycle++) {
        $root = Join-Path $testRoot "window-$cycle"
        $null = [IO.Directory]::CreateDirectory($root)
        $timer = [Diagnostics.Stopwatch]::StartNew()
        $process = Start-Isolated $desktop $root '' $false $true
        try {
            $deadline = [DateTime]::UtcNow.AddSeconds(30)
            while (![IO.File]::Exists((Join-Path $root 'ui-ready.json')) -or ![IO.File]::Exists((Join-Path $root 'read-ok.json'))) {
                Require (!$process.HasExited) 'Desktop exited before the page became ready'
                Require ([DateTime]::UtcNow -lt $deadline) 'Desktop did not become ready; inspect its test window'
                Start-Sleep -Milliseconds 100
            }
            $readyMs = $timer.Elapsed.TotalMilliseconds
            if ($cycle -eq 1) {
                $process.Refresh()
                Check-Duplicate $root '--compatibility' $process.MainWindowHandle
                Check-Duplicate $root '' $process.MainWindowHandle
                $output = Join-Path $root 'while-gui-open.json'
                $run = Run-Cli $desktop $root ('--headless info --result ' + (Quote-Argument $output))
                Require ($run.exitCode -eq 0 -and [IO.File]::Exists($output)) 'Open GUI intercepted a headless command'
            }
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
            [SetupWindowClose]::Request($process.MainWindowHandle, $process.Id)
            Require ($process.WaitForExit(10000)) 'Desktop did not exit after normal close'
            $deadline = [DateTime]::UtcNow.AddSeconds(10)
            do {
                $remaining = @($handles | Where-Object { !$_.HasExited })
                if ($remaining.Count) { Start-Sleep -Milliseconds 100 }
            } while ($remaining.Count -and [DateTime]::UtcNow -lt $deadline)
            Require ($remaining.Count -eq 0) 'A process from this UI tree remained after close'
            foreach ($handle in $handles) { $handle.Dispose() }
            Require (![IO.Directory]::Exists((Join-Path $root 'state'))) 'Read-only GUI wrote deployment state'
        } finally {
            if (!$process.HasExited) { $null = $process.CloseMainWindow() }
            $process.Dispose()
        }
    }
    Write-Host 'PASS desktop lifecycle: embedded page, real native command, idle without bridge polling, three normal closes with no remaining owned process'
}
$report = [ordered]@{
    schemaVersion=1
    sizes=[ordered]@{nativeBytes=(Get-Item (Join-Path $ProjectRoot 'work\ui-native\ChatGPTLauncherSetup.exe')).Length; tauriBytes=(Get-Item $production).Length; launcherBytes=(Get-Item (Join-Path $ProjectRoot 'outputs\ChatGPTProxyLauncher.exe')).Length}
    cli=$timings
    gui=$gui
    compatibility=$compatibility
}
[IO.File]::WriteAllText((Join-Path $testRoot 'summary.json'), ($report | ConvertTo-Json -Depth 10))
Write-Host "Local measurements: $testRoot\summary.json"
