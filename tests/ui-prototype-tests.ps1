# SPDX-License-Identifier: GPL-3.0-only
param(
    [ValidateSet('Smoke','Start','Inspect','Resize','RequestClose','VerifyClosed')][string]$Action='Smoke',
    [string]$Session,
    [switch]$RemoteDebugging,
    [switch]$Functional,
    [switch]$Sample,
    [ValidateRange(0,5000)][int]$SaveDelayMilliseconds=0,
    [ValidateRange(1,600)][int]$IdleSeconds=60,
    [ValidateRange(1,30)][int]$Cycles=3,
    [ValidateRange(480,1600)][int]$Width=640,
    [ValidateRange(400,1200)][int]$Height=760,
    [ValidateRange(1,30)][int]$ExpectedReads=1
)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'ui-process-job.ps1')
$projectRoot=Split-Path $PSScriptRoot
if($Sample -and $Functional) { throw 'Select either the sample or the functional build' }
$exe=Join-Path $projectRoot $(if($Sample) {'work\ui-sample\ChatGPTLauncherSetup-Sample.exe'} elseif($Functional) {'work\ui-stage1-test\ChatGPTLauncherSetup.exe'} else {'work\ui-prototype\ChatGPTLauncherSetup.exe'})
$parent=Join-Path $projectRoot $(if($Sample) {'work\ui-sample-tests'} elseif($Functional) {'work\ui-functional-tests'} else {'work\ui-prototype-tests'})
function Require([bool]$Value, [string]$Message) { if (!$Value) { throw $Message } }
if (!('PrototypeCloseMessage' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
public static class PrototypeCloseMessage {
    [DllImport("user32.dll", SetLastError=true)]
    static extern bool PostMessageW(IntPtr window, uint message, UIntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")]
    static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);
    [DllImport("user32.dll")]
    public static extern uint GetDpiForWindow(IntPtr window);
    [DllImport("user32.dll", SetLastError=true)]
    static extern bool SetWindowPos(IntPtr window, IntPtr after, int x, int y, int width, int height, uint flags);
    public static void Resize(IntPtr window, int expectedProcessId, int width, int height) {
        uint owner;
        if (window == IntPtr.Zero || GetWindowThreadProcessId(window, out owner) == 0 || owner != expectedProcessId)
            throw new InvalidOperationException("Unexpected preview window owner");
        // Resize only this test window, in physical pixels. Keep position, z-order and activation.
        if (!SetWindowPos(window, IntPtr.Zero, 0, 0, width, height, 0x0016))
            throw new Win32Exception(Marshal.GetLastWin32Error());
    }
    public static void Request(IntPtr window, int expectedProcessId) {
        uint owner;
        if (window == IntPtr.Zero || GetWindowThreadProcessId(window, out owner) == 0 || owner != expectedProcessId)
            throw new InvalidOperationException("Unexpected prototype window owner");
        // Match the title-bar/system-menu close action, and check message delivery.
        if (!PostMessageW(window, 0x0112, new UIntPtr(0xF060), IntPtr.Zero))
            throw new Win32Exception(Marshal.GetLastWin32Error());
    }
}
'@
}
function Tree([int]$RootId, [object[]]$Rows=@(Get-CimInstance Win32_Process -Property ProcessId,ParentProcessId,Name,CreationDate)) {
    $byId=@{}
    foreach($row in $Rows) { $byId[[int]$row.ProcessId]=$row }
    $ids=[Collections.Generic.HashSet[int]]::new(); $null=$ids.Add($RootId)
    do {
        $added=$false
        foreach($row in $Rows) {
            $parent=$byId[[int]$row.ParentProcessId]
            # ParentProcessId can point at a newer, unrelated process after PID reuse.
            if($parent -and $row.CreationDate -ge $parent.CreationDate -and
                $ids.Contains([int]$row.ParentProcessId) -and $ids.Add([int]$row.ProcessId)) { $added=$true }
        }
    } while($added)
    return @($Rows | Where-Object { $ids.Contains([int]$_.ProcessId) })
}
function Assert-UiSessionDirectory([string]$Path) {
    for ($component=[IO.DirectoryInfo]::new($Path); $null -ne $component; $component=$component.Parent) {
        Require ($component.Exists -and !($component.Attributes -band [IO.FileAttributes]::ReparsePoint)) 'Session directory is redirected'
    }
}
function New-UiSessionDirectory {
    Assert-UiSessionDirectory ([IO.Path]::GetDirectoryName($parent))
    $null=[IO.Directory]::CreateDirectory($parent)
    Assert-UiSessionDirectory $parent
    $path=Join-Path $parent ([guid]::NewGuid().ToString('N'))
    $null=New-Item -ItemType Directory -Path $path -ErrorAction Stop
    Assert-UiSessionDirectory $path
    return $path
}
function Test-StandaloneSampleProfile {
    $sandbox = New-UiSessionDirectory
    $temp = Join-Path $sandbox 'temp'
    $outside = Join-Path $sandbox 'outside'
    $null = [IO.Directory]::CreateDirectory($temp)
    $null = [IO.Directory]::CreateDirectory($outside)
    $guard = Join-Path $outside 'guard'
    [IO.File]::WriteAllText($guard, 'unchanged')
    $prefix = 'ChatGPTLauncherSetup-CompanionSample-Cache-'
    $null = New-Item -ItemType Junction -Path (Join-Path $temp ($prefix + '0')) -Target $outside
    $first = Join-Path $temp ($prefix + '1')
    $next = Join-Path $temp ($prefix + '2')
    foreach ($run in 1..3) {
        if ($run -eq 3) {
            $null = New-Item -ItemType HardLink -Path (Join-Path $first 'linked-file') -Target $guard
        }
        $session = New-UiSessionDirectory
        $info = [Diagnostics.ProcessStartInfo]::new()
        $info.FileName = $exe; $info.WorkingDirectory = $session; $info.UseShellExecute = $false
        $info.EnvironmentVariables['TEMP'] = $temp
        $info.EnvironmentVariables['TMP'] = $temp
        $info.EnvironmentVariables.Remove('CHATGPT_SETUP_TEST_ROOT')
        $process = Start-UiTrackedProcess $info
        try {
            $started = $process.StartTime.ToUniversalTime().Ticks
            $deadline = [DateTime]::UtcNow.AddSeconds(30)
            do {
                $process.Refresh()
                Require (!$process.HasExited -and [DateTime]::UtcNow -lt $deadline) 'Standalone sample did not open'
                if (!$process.MainWindowHandle.ToInt64()) { Start-Sleep -Milliseconds 100 }
            } while (!$process.MainWindowHandle.ToInt64())
            [PrototypeCloseMessage]::Request($process.MainWindowHandle, $process.Id)
            Assert-UiJobExited $session $process.Id $started
        } finally {
            if (!$process.HasExited) { $null = $process.CloseMainWindow() }
            $process.Dispose()
        }
        $expected = if ($run -eq 3) { $next } else { $first }
        Require ([IO.Directory]::Exists((Join-Path $expected 'EBWebView'))) 'Standalone sample used the wrong WebView2 profile'
        $count = if ($run -eq 3) { 3 } else { 2 }
        Require ([IO.Directory]::GetDirectories($temp, ($prefix + '*')).Count -eq $count) 'Standalone sample kept creating profiles'
    }
    Require ([IO.Directory]::GetFileSystemEntries($outside).Count -eq 1 -and [IO.File]::ReadAllText($guard) -ceq 'unchanged') 'An external file changed'
    Write-Host 'PASS standalone sample reuses one browser profile and skips junctions and hard-linked caches'
}
function Start-Preview {
    Require ([IO.File]::Exists($exe)) 'Build the prototype first'
    $root=New-UiSessionDirectory
    $info=[Diagnostics.ProcessStartInfo]::new()
    $info.FileName=$exe; $info.WorkingDirectory=$root; $info.UseShellExecute=$false
    $info.EnvironmentVariables['CHATGPT_SETUP_TEST_ROOT']=$root
    if($Functional -and $SaveDelayMilliseconds) { $info.EnvironmentVariables['CHATGPT_SETUP_TEST_SAVE_DELAY_MS']=[string]$SaveDelayMilliseconds }
    $port=0
    if($RemoteDebugging) {
        $listener=[Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback,0)
        $listener.Start(); $port=$listener.LocalEndpoint.Port; $listener.Stop()
        $info.EnvironmentVariables['WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS']="--remote-debugging-port=$port --remote-debugging-address=127.0.0.1"
    }
    $timer=[Diagnostics.Stopwatch]::StartNew()
    $process=Start-UiTrackedProcess $info
    try {
        $deadline=[DateTime]::UtcNow.AddSeconds(30)
        while(![IO.File]::Exists((Join-Path $root 'ui-ready.json'))) {
            Require (!$process.HasExited -and [DateTime]::UtcNow -lt $deadline) 'Prototype did not become ready'
            Start-Sleep -Milliseconds 100
        }
        if($Functional) {
            while(![IO.File]::Exists((Join-Path $root 'actions.json'))) {
                Require (!$process.HasExited -and [DateTime]::UtcNow -lt $deadline) 'Initial native validation did not finish'
                Start-Sleep -Milliseconds 50
            }
        }
        $metadata=@{processId=$process.Id; started=$process.StartTime.ToUniversalTime().Ticks; readyMs=$timer.Elapsed.TotalMilliseconds; port=$port; exe=$exe; root=$root}
        Write-UiTestReport (Join-Path $root 'session.json') ($metadata | ConvertTo-Json)
        return $metadata
    } catch {
        if(!$process.HasExited) { $null=$process.CloseMainWindow() }
        throw
    } finally { $process.Dispose() }
}
function Resolve-Session {
    $path=[IO.Path]::GetFullPath($Session)
    $leaf=[IO.Path]::GetFileName($path.TrimEnd('\'))
    $path=$path.TrimEnd('\')
    Require ($leaf -cmatch '^[a-f0-9]{32}$' -and [IO.Path]::GetDirectoryName($path) -ieq $parent) 'Session must be an isolated prototype test'
    $path=Join-Path $parent $leaf
    Assert-UiSessionDirectory $path
    $metadata=[IO.File]::ReadAllText((Join-Path $path 'session.json')) | ConvertFrom-Json
    Require ($metadata.root -ceq $path) 'Session metadata does not match its directory'
    $metadata.root=$path
    return $metadata
}
function Owned-Process($Metadata) {
    $process=Get-Process -Id $Metadata.processId -ErrorAction SilentlyContinue
    if(!$process) { return $null }
    Require ($process.StartTime.ToUniversalTime().Ticks -eq $Metadata.started -and $process.Path -eq $exe) 'Process identity changed'
    # Keep a queryable handle before closing; reopening an exited PID can lose its exit code.
    $null=$process.Handle
    return $process
}
function Inspect-Preview($Metadata, [int]$Reads=1) {
    $owner=Owned-Process $Metadata
    Require ($null -ne $owner) 'Preview exited before inspection'
    try {
        $dpi=[PrototypeCloseMessage]::GetDpiForWindow($owner.MainWindowHandle)
        Require ($dpi -gt 0) 'Preview has no visible main window'
    } finally { $owner.Dispose() }
    if(!$Functional) { Require (![IO.Directory]::Exists((Join-Path $Metadata.root 'state'))) 'Prototype wrote deployment state' }
    if($Functional) {
        $read=Get-Content -LiteralPath (Join-Path $Metadata.root 'read-ok.json') -Raw | ConvertFrom-Json
        Require ($read.reads -eq $Reads) 'Unexpected number of operational page loads'
    } else { Require (![IO.File]::Exists((Join-Path $Metadata.root 'read-ok.json'))) 'Prototype called the real read bridge' }
    $rows=@(Tree $Metadata.processId)
    $samples=@(foreach($row in $rows) {
        $process=Get-Process -Id $row.ProcessId -ErrorAction Stop
        try { [pscustomobject]@{id=$process.Id; started=$process.StartTime.ToUniversalTime().Ticks; working=$process.WorkingSet64; private=$process.PrivateMemorySize64;
            handles=$process.HandleCount; threads=$process.Threads.Count; cpuMs=$process.TotalProcessorTime.TotalMilliseconds} }
        finally { $process.Dispose() }
    })
    $hostSample=$samples | Where-Object { $_.id -eq $Metadata.processId -and $_.started -eq $Metadata.started }
    Require ($null -ne $hostSample) 'Preview owner changed while sampling'
    Write-UiTestReport (Join-Path $Metadata.root 'tree.json') (ConvertTo-Json -InputObject $samples)
    return @{processes=$samples.Count; privateMiB=[Math]::Round(($samples | Measure-Object private -Sum).Sum/1MB,2);
        hostPrivateMiB=[Math]::Round($hostSample.private/1MB,2); hostThreads=$hostSample.threads;
        workingMiB=[Math]::Round(($samples | Measure-Object working -Sum).Sum/1MB,2); readyMs=[Math]::Round($Metadata.readyMs,2);
        handles=($samples | Measure-Object handles -Sum).Sum; threads=($samples | Measure-Object threads -Sum).Sum;
        cpuMs=[Math]::Round(($samples | Measure-Object cpuMs -Sum).Sum,2); windowDpi=$dpi}
}
function Read-SampledTree($Metadata) {
    # Windows PowerShell 5.1 emits a JSON array as one object. Unroll its rows on return.
    $rows=Get-Content -LiteralPath (Join-Path $Metadata.root 'tree.json') -Raw | ConvertFrom-Json
    $owners=@($rows | Where-Object { $_.id -eq $Metadata.processId -and $_.started -eq $Metadata.started })
    Require ($owners.Count -eq 1) 'Process snapshot is missing its owner'
    return $rows
}
function Get-LiveSampleIds([object[]]$Rows) {
    foreach($row in $Rows) {
        $process=Get-Process -Id ([int]$row.id) -ErrorAction SilentlyContinue
        if($process) {
            try { if(!$process.HasExited -and $process.StartTime.ToUniversalTime().Ticks -eq $row.started) { $process.Id } }
            finally { $process.Dispose() }
        }
    }
}
function Verify-Closed($Metadata) {
    Assert-UiJobExited $Metadata.root $Metadata.processId $Metadata.started
    $events=Get-Content -LiteralPath (Join-Path $Metadata.root 'close-events.json') -Raw | ConvertFrom-Json
    Require ($events.approved -ge 1) 'The page did not approve a normal close'
    if(!$Functional) { Require (![IO.Directory]::Exists((Join-Path $Metadata.root 'state'))) 'Prototype wrote deployment state' }
    Write-Host 'PASS normal close with no remaining owned process; isolated session'
}
switch($Action) {
    'Start' { Start-Preview | ConvertTo-Json }
    'Inspect' { Inspect-Preview (Resolve-Session) $ExpectedReads | ConvertTo-Json }
    'Resize' {
        $process=Owned-Process (Resolve-Session)
        Require ($null -ne $process) 'Preview has already exited'
        try { [PrototypeCloseMessage]::Resize($process.MainWindowHandle,$process.Id,$Width,$Height) } finally { $process.Dispose() }
    }
    'RequestClose' {
        $process=Owned-Process (Resolve-Session)
        Require ($null -ne $process) 'Prototype has already exited'
        try { [PrototypeCloseMessage]::Request($process.MainWindowHandle,$process.Id) } finally { $process.Dispose() }
    }
    'VerifyClosed' { Verify-Closed (Resolve-Session) }
    'Smoke' {
        if ($Sample) { Test-StandaloneSampleProfile }
        & {
            $Session=New-UiSessionDirectory
            $text=[string][char]0x4E2D+[char]0x6587
            $metaFile=Join-Path $Session 'session.json'
            Write-UiTestReport $metaFile (@{root=$parent; text=$text} | ConvertTo-Json)
            $refused=$false
            try { $null=Resolve-Session } catch { $refused=$true }
            Require $refused 'Session metadata redirected reports'
            Write-UiTestReport $metaFile (@{root=$Session; text=$text} | ConvertTo-Json)
            Require ((Resolve-Session).text -ceq $text) 'Session metadata did not preserve Unicode'
            $sentinel=Join-Path $Session 'sentinel'
            [IO.File]::WriteAllText($sentinel,'keep')
            $linked=Join-Path $Session 'tree.json'
            $null=New-Item -ItemType HardLink -Path $linked -Target $sentinel
            Write-UiTestReport $linked '[]'
            Require ([IO.File]::ReadAllText($sentinel) -ceq 'keep') 'Session report changed the old link target'
        }
        $sampleTime=[datetime]'2026-01-01T00:00:00Z'
        $sampleTree=@(
            [pscustomobject]@{ProcessId=1;ParentProcessId=0;CreationDate=$sampleTime},
            [pscustomobject]@{ProcessId=2;ParentProcessId=1;CreationDate=$sampleTime.AddSeconds(-2)},
            [pscustomobject]@{ProcessId=3;ParentProcessId=1;CreationDate=$sampleTime.AddSeconds(1)},
            [pscustomobject]@{ProcessId=4;ParentProcessId=2;CreationDate=$sampleTime.AddSeconds(2)},
            [pscustomobject]@{ProcessId=5;ParentProcessId=3;CreationDate=$sampleTime.AddSeconds(2)}
        )
        Require (((Tree 1 $sampleTree).ProcessId -join ',') -eq '1,3,5') 'PID reuse included an unrelated process tree'
        $measurements=@()
        for($cycle=1;$cycle -le $Cycles;$cycle++) {
            $metadata=Start-Preview
            $initial=Inspect-Preview $metadata
            $initialTree=@(Read-SampledTree $metadata)
            Require (@(Get-LiveSampleIds $initialTree) -contains $metadata.processId) 'Live owner was reported as closed'
            $initialIdentities=($initialTree |
                Sort-Object id | ForEach-Object { "$($_.id):$($_.started)" }) -join ','
            $seconds=if($cycle -eq 1) { $IdleSeconds } else { 2 }
            Write-Host "Prototype cycle $cycle ready; observing $seconds seconds"
            $actionsBefore=if($Functional) { [IO.File]::ReadAllText((Join-Path $metadata.root 'actions.json')) } else { '' }
            for($second=0;$second -lt $seconds;$second++) { Start-Sleep -Seconds 1 }
            $metrics=Inspect-Preview $metadata
            $metrics.initialPrivateMiB=$initial.privateMiB
            $metrics.idleSeconds=$seconds
            $finalIdentities=(Read-SampledTree $metadata |
                Sort-Object id | ForEach-Object { "$($_.id):$($_.started)" }) -join ','
            $metrics.processTreeChanged=$initialIdentities -cne $finalIdentities
            # Lifetime CPU sums are comparable only while the exact process set stays the same.
            $metrics.idleCpuMs=if ($metrics.processTreeChanged) { $null } else { [Math]::Round($metrics.cpuMs-$initial.cpuMs,2) }
            $metrics.cycle=$cycle
            if($Functional) { Require ([IO.File]::ReadAllText((Join-Path $metadata.root 'actions.json')) -eq $actionsBefore) 'Idle page repeated a native command' }
            Write-UiTestReport (Join-Path $metadata.root 'metrics.json') ($metrics | ConvertTo-Json)
            Write-Output ($metrics | ConvertTo-Json -Compress)
            $process=Owned-Process $metadata
            try {
                [PrototypeCloseMessage]::Request($process.MainWindowHandle,$process.Id)
                Require ($process.WaitForExit(10000)) 'Clean prototype close did not exit'
                Require ($process.ExitCode -eq 0) ('Prototype exit code: ' + $process.ExitCode)
            } finally { $process.Dispose() }
            Verify-Closed $metadata
            $closeEvents=Get-Content -LiteralPath (Join-Path $metadata.root 'close-events.json') -Raw | ConvertFrom-Json
            Require ($closeEvents.requested -eq 1 -and $closeEvents.approved -eq 1) 'Native close did not pass through the page exactly once'
            $measurements+=$metrics
        }
        $report=Join-Path $parent ('lifecycle-'+[guid]::NewGuid().ToString('N')+'.json')
        Write-UiTestReport $report (ConvertTo-Json -InputObject $measurements)
        Write-Host "PASS embedded page, event permissions, $Cycles native closes and isolated lifecycle"
        Write-Host "Local measurements: $report"
    }
}
