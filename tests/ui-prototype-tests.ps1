# SPDX-License-Identifier: GPL-3.0-only
param(
    [ValidateSet('Smoke','Start','Inspect','RequestClose','VerifyClosed')][string]$Action='Smoke',
    [string]$Session,
    [switch]$RemoteDebugging,
    [switch]$Functional,
    [ValidateRange(0,5000)][int]$SaveDelayMilliseconds=0,
    [ValidateRange(1,60)][int]$IdleSeconds=60
)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot
$exe=Join-Path $projectRoot $(if($Functional) {'work\ui-stage1-test\ChatGPTLauncherSetup.exe'} else {'work\ui-prototype\ChatGPTLauncherSetup.exe'})
$parent=Join-Path $projectRoot $(if($Functional) {'work\ui-functional-tests'} else {'work\ui-prototype-tests'})
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
function Start-Preview {
    Require ([IO.File]::Exists($exe)) 'Build the prototype first'
    $root=Join-Path $parent ([guid]::NewGuid().ToString('N'))
    $null=[IO.Directory]::CreateDirectory($root)
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
    $process=[Diagnostics.Process]::Start($info)
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
        [IO.File]::WriteAllText((Join-Path $root 'session.json'),($metadata | ConvertTo-Json))
        return $metadata
    } catch {
        if(!$process.HasExited) { $null=$process.CloseMainWindow() }
        throw
    } finally { $process.Dispose() }
}
function Resolve-Session {
    $path=[IO.Path]::GetFullPath($Session)
    Require ($path.StartsWith($parent+'\',[StringComparison]::OrdinalIgnoreCase)) 'Session must be an isolated prototype test'
    return Get-Content -LiteralPath (Join-Path $path 'session.json') -Raw | ConvertFrom-Json
}
function Owned-Process($Metadata) {
    $process=Get-Process -Id $Metadata.processId -ErrorAction SilentlyContinue
    if(!$process) { return $null }
    Require ($process.StartTime.ToUniversalTime().Ticks -eq $Metadata.started -and $process.Path -eq $exe) 'Process identity changed'
    # Keep a queryable handle before closing; reopening an exited PID can lose its exit code.
    $null=$process.Handle
    return $process
}
function Inspect-Preview($Metadata) {
    if(!$Functional) { Require (![IO.Directory]::Exists((Join-Path $Metadata.root 'state'))) 'Prototype wrote deployment state' }
    if($Functional) {
        $read=Get-Content -LiteralPath (Join-Path $Metadata.root 'read-ok.json') -Raw | ConvertFrom-Json
        Require ($read.reads -eq 1) 'Operational page must load once, without polling'
    } else { Require (![IO.File]::Exists((Join-Path $Metadata.root 'read-ok.json'))) 'Prototype called the real read bridge' }
    $rows=@(Tree $Metadata.processId)
    $samples=@(foreach($row in $rows) {
        $process=Get-Process -Id $row.ProcessId -ErrorAction Stop
        try { [pscustomobject]@{id=$process.Id; started=$process.StartTime.ToUniversalTime().Ticks; working=$process.WorkingSet64; private=$process.PrivateMemorySize64} }
        finally { $process.Dispose() }
    })
    [IO.File]::WriteAllText((Join-Path $Metadata.root 'tree.json'),(ConvertTo-Json -InputObject $samples))
    return @{processes=$samples.Count; privateMiB=[Math]::Round(($samples | Measure-Object private -Sum).Sum/1MB,2);
        workingMiB=[Math]::Round(($samples | Measure-Object working -Sum).Sum/1MB,2); readyMs=[Math]::Round($Metadata.readyMs,2)}
}
function Verify-Closed($Metadata) {
    $rows=@(Get-Content -LiteralPath (Join-Path $Metadata.root 'tree.json') -Raw | ConvertFrom-Json)
    $deadline=[DateTime]::UtcNow.AddSeconds(10)
    do {
        $remaining=@(foreach($row in $rows) {
            $process=Get-Process -Id $row.id -ErrorAction SilentlyContinue
            if($process) { try { if($process.StartTime.ToUniversalTime().Ticks -eq $row.started) { $process.Id } } finally { $process.Dispose() } }
        })
        if($remaining.Count) { Start-Sleep -Milliseconds 100 }
    } while($remaining.Count -and [DateTime]::UtcNow -lt $deadline)
    Require ($remaining.Count -eq 0) 'An owned prototype process remained after close'
    if(!$Functional) { Require (![IO.Directory]::Exists((Join-Path $Metadata.root 'state'))) 'Prototype wrote deployment state' }
    Write-Host 'PASS normal close with no remaining owned process; isolated session'
}
switch($Action) {
    'Start' { Start-Preview | ConvertTo-Json }
    'Inspect' { Inspect-Preview (Resolve-Session) | ConvertTo-Json }
    'RequestClose' {
        $process=Owned-Process (Resolve-Session)
        Require ($null -ne $process) 'Prototype has already exited'
        try { [PrototypeCloseMessage]::Request($process.MainWindowHandle,$process.Id) } finally { $process.Dispose() }
    }
    'VerifyClosed' { Verify-Closed (Resolve-Session) }
    'Smoke' {
        $sampleTime=[datetime]'2026-01-01T00:00:00Z'
        $sampleTree=@(
            [pscustomobject]@{ProcessId=1;ParentProcessId=0;CreationDate=$sampleTime},
            [pscustomobject]@{ProcessId=2;ParentProcessId=1;CreationDate=$sampleTime.AddSeconds(-2)},
            [pscustomobject]@{ProcessId=3;ParentProcessId=1;CreationDate=$sampleTime.AddSeconds(1)},
            [pscustomobject]@{ProcessId=4;ParentProcessId=2;CreationDate=$sampleTime.AddSeconds(2)},
            [pscustomobject]@{ProcessId=5;ParentProcessId=3;CreationDate=$sampleTime.AddSeconds(2)}
        )
        Require (((Tree 1 $sampleTree).ProcessId -join ',') -eq '1,3,5') 'PID reuse included an unrelated process tree'
        for($cycle=1;$cycle -le 3;$cycle++) {
            $metadata=Start-Preview
            $sample=Inspect-Preview $metadata
            $seconds=if($cycle -eq 1) { $IdleSeconds } else { 2 }
            Write-Host "Prototype cycle $cycle ready; observing $seconds seconds"
            $actionsBefore=if($Functional) { [IO.File]::ReadAllText((Join-Path $metadata.root 'actions.json')) } else { '' }
            for($second=0;$second -lt $seconds;$second++) { Start-Sleep -Seconds 1 }
            $sample=Inspect-Preview $metadata
            if($Functional) { Require ([IO.File]::ReadAllText((Join-Path $metadata.root 'actions.json')) -eq $actionsBefore) 'Idle page repeated a native command' }
            [IO.File]::WriteAllText((Join-Path $metadata.root 'metrics.json'),($sample | ConvertTo-Json))
            Write-Output ($sample | ConvertTo-Json -Compress)
            $process=Owned-Process $metadata
            try {
                [PrototypeCloseMessage]::Request($process.MainWindowHandle,$process.Id)
                Require ($process.WaitForExit(10000)) 'Clean prototype close did not exit'
                Require ($process.ExitCode -eq 0) ('Prototype exit code: ' + $process.ExitCode)
            } finally { $process.Dispose() }
            Verify-Closed $metadata
            $closeEvents=Get-Content -LiteralPath (Join-Path $metadata.root 'close-events.json') -Raw | ConvertFrom-Json
            Require ($closeEvents.requested -eq 1 -and $closeEvents.approved -eq 1) 'Native close did not pass through the page exactly once'
        }
        Write-Host 'PASS embedded prototype, event permissions, three native closes and isolated lifecycle'
    }
}
