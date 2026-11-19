# SPDX-License-Identifier: GPL-3.0-only
param([switch]$Observe, [switch]$LateChildFixture, [switch]$WaitForReleaseFixture, [int]$FixtureExitCode = 0)

if ($WaitForReleaseFixture) {
    $release = Join-Path ([Environment]::CurrentDirectory) 'release-child'
    $deadline = [DateTime]::UtcNow.AddSeconds(20)
    while (![IO.File]::Exists($release)) {
        if ([DateTime]::UtcNow -ge $deadline) { throw 'Late child was not released' }
        Start-Sleep -Milliseconds 50
    }
    exit $FixtureExitCode
}

if (!('SetupUiTestJob' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;
public static class SetupUiTestJob {
    [StructLayout(LayoutKind.Sequential)]
    struct Accounting {
        public long user, kernel, periodUser, periodKernel;
        public uint faults, total, active, terminated;
    }
    [DllImport("kernel32.dll", SetLastError=true)]
    static extern SafeFileHandle CreateJobObjectW(IntPtr attributes, IntPtr name);
    [DllImport("kernel32.dll", SetLastError=true)]
    static extern bool AssignProcessToJobObject(SafeFileHandle job, IntPtr process);
    [DllImport("kernel32.dll")]
    static extern IntPtr GetCurrentProcess();
    [DllImport("kernel32.dll", SetLastError=true)]
    static extern bool QueryInformationJobObject(SafeFileHandle job, int kind, out Accounting result, int size, IntPtr used);
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    public static extern bool MoveFileExW(string source, string destination, uint flags);
    public static SafeFileHandle TrackCurrentProcess() {
        var job = CreateJobObjectW(IntPtr.Zero, IntPtr.Zero);
        if (job.IsInvalid || !AssignProcessToJobObject(job, GetCurrentProcess())) {
            int error = Marshal.GetLastWin32Error();
            job.Dispose();
            throw new Win32Exception(error);
        }
        return job;
    }
    public static uint Active(SafeFileHandle job) {
        Accounting result;
        if (!QueryInformationJobObject(job, 1, out result, Marshal.SizeOf(typeof(Accounting)), IntPtr.Zero))
            throw new Win32Exception(Marshal.GetLastWin32Error());
        return result.active;
    }
}
'@
}

function Write-UiTestReport([string]$Path, [string]$Json) {
    $temporary = Join-Path ([IO.Path]::GetDirectoryName($Path)) ([guid]::NewGuid().ToString('N') + '.tmp')
    $stream = [IO.File]::Open($temporary, 'CreateNew', 'Write', 'None')
    try {
        try {
            $bytes = [Text.Encoding]::UTF8.GetBytes($Json)
            $stream.Write($bytes, 0, $bytes.Length)
        } finally { $stream.Dispose() }
        # Replace the directory entry itself, including when it is a link.
        $deadline = [DateTime]::UtcNow.AddSeconds(1)
        while (!([SetupUiTestJob]::MoveFileExW($temporary, $Path, 1))) {
            $code = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
            if ($code -ne 32 -or [DateTime]::UtcNow -ge $deadline) { throw [ComponentModel.Win32Exception]::new($code) }
            Start-Sleep -Milliseconds 10
        }
        $temporary = $null
    } finally { if ($temporary) { [IO.File]::Delete($temporary) } }
}

if ($LateChildFixture) {
    $folder = [Environment]::CurrentDirectory
    $deadline = [DateTime]::UtcNow.AddSeconds(15)
    while (![IO.File]::Exists((Join-Path $folder 'inspected'))) {
        if ([DateTime]::UtcNow -ge $deadline) { throw 'Late-child fixture timed out' }
        Start-Sleep -Milliseconds 50
    }
    $arguments = '-NoProfile -NonInteractive -ExecutionPolicy Bypass -File "' + $PSCommandPath + '" -WaitForReleaseFixture'
    $child = Start-Process -FilePath (Join-Path $PSHOME 'powershell.exe') -ArgumentList $arguments -WorkingDirectory $folder -WindowStyle Hidden -PassThru
    try {
        Write-UiTestReport (Join-Path $folder 'late-child.json') (@{id=$child.Id; started=$child.StartTime.ToUniversalTime().Ticks} | ConvertTo-Json)
    } finally { $child.Dispose() }
    return
}

# An observer owns an unnamed Job for this one session. It joins before starting the UI,
# so children created at any point are tracked even across separate Inspect and Verify calls.
# The Job has no breakaway or kill-on-close flags. Production processes never enter it.
if ($Observe) {
    $ErrorActionPreference = 'Stop'
    $folder = [Environment]::CurrentDirectory
    $job = [SetupUiTestJob]::TrackCurrentProcess()
    try {
        $target = [Diagnostics.ProcessStartInfo]::new()
        $target.FileName = $env:CHATGPT_TEST_TRACKED_EXE
        $target.Arguments = $env:CHATGPT_TEST_TRACKED_ARGS
        $target.UseShellExecute = $false
        $target.CreateNoWindow = $true
        $target.WorkingDirectory = $folder
        $target.EnvironmentVariables.Remove('CHATGPT_TEST_TRACKED_EXE')
        $target.EnvironmentVariables.Remove('CHATGPT_TEST_TRACKED_ARGS')
        $process = [Diagnostics.Process]::Start($target)
        try {
            $identity = @{processId=$process.Id; started=$process.StartTime.ToUniversalTime().Ticks}
            Write-UiTestReport (Join-Path $folder 'job-started.json') ($identity | ConvertTo-Json)
            $process.WaitForExit()
            $identity.exitCode = $process.ExitCode
            $deadline = [DateTime]::UtcNow.AddSeconds(10)
            do {
                $remaining = [SetupUiTestJob]::Active($job) - 1 # Exclude this observer.
                if ($remaining) { Start-Sleep -Milliseconds 75 }
            } while ($remaining -and [DateTime]::UtcNow -lt $deadline)
            $identity.remaining = $remaining
            Write-UiTestReport (Join-Path $folder 'job-exit.json') ($identity | ConvertTo-Json)
        } finally { $process.Dispose() }
    } finally { $job.Dispose() }
    return
}

function Start-UiTrackedProcess([Diagnostics.ProcessStartInfo]$Info) {
    $observer = [Diagnostics.ProcessStartInfo]::new()
    $shellName = if ($PSVersionTable.PSEdition -eq 'Core') { 'pwsh.exe' } else { 'powershell.exe' }
    $observer.FileName = Join-Path $PSHOME $shellName
    $observer.Arguments = '-NoProfile -NonInteractive -ExecutionPolicy Bypass -File "' + (Join-Path $PSScriptRoot 'ui-process-job.ps1') + '" -Observe'
    $observer.UseShellExecute = $false
    $observer.CreateNoWindow = $true
    $observer.WindowStyle = 'Hidden'
    $observer.WorkingDirectory = $Info.WorkingDirectory
    $observer.EnvironmentVariables.Clear()
    foreach ($key in $Info.EnvironmentVariables.Keys) { $observer.EnvironmentVariables[$key] = $Info.EnvironmentVariables[$key] }
    $observer.EnvironmentVariables['CHATGPT_TEST_TRACKED_EXE'] = $Info.FileName
    $observer.EnvironmentVariables['CHATGPT_TEST_TRACKED_ARGS'] = $Info.Arguments
    $hostProcess = [Diagnostics.Process]::Start($observer)
    try {
        $startedFile = Join-Path $Info.WorkingDirectory 'job-started.json'
        $deadline = [DateTime]::UtcNow.AddSeconds(30)
        while (![IO.File]::Exists($startedFile)) {
            if ($hostProcess.HasExited -or [DateTime]::UtcNow -ge $deadline) { throw 'Isolated process observer did not start' }
            Start-Sleep -Milliseconds 50
        }
        $identity = [IO.File]::ReadAllText($startedFile) | ConvertFrom-Json
        $child = [Diagnostics.Process]::GetProcessById($identity.processId)
        if ($child.StartTime.ToUniversalTime().Ticks -ne $identity.started -or $child.MainModule.FileName -ine $Info.FileName) {
            $child.Dispose()
            throw 'Unexpected isolated process identity'
        }
        $null = $child.Handle
        return $child
    } finally { $hostProcess.Dispose() }
}

function Assert-UiJobExited([string]$Root, [int]$ProcessId, [long]$Started) {
    $path = Join-Path $Root 'job-exit.json'
    $deadline = [DateTime]::UtcNow.AddSeconds(13)
    while (![IO.File]::Exists($path)) {
        if ([DateTime]::UtcNow -ge $deadline) { throw 'No exit observation for this isolated Job' }
        Start-Sleep -Milliseconds 75
    }
    $report = [IO.File]::ReadAllText($path) | ConvertFrom-Json
    if ($report.processId -ne $ProcessId -or $report.started -ne $Started -or $null -eq $report.remaining -or $report.remaining -ne 0) {
        throw 'The isolated Job did not confirm that all its UI processes exited'
    }
    if ($null -eq $report.exitCode -or $report.exitCode -ne 0) {
        throw "The isolated UI did not exit successfully (exit code: $($report.exitCode))"
    }
}
