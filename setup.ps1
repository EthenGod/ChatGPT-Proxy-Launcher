# SPDX-License-Identifier: GPL-3.0-only
# Keep this entry point ASCII so Windows PowerShell can execute it from an HTTP response.
[CmdletBinding()]
param([switch]$Headless, [string]$Request, [string]$Result,
    [ValidateSet('deploy', 'plan', 'info')][string]$Operation = 'deploy')
& {
    $ErrorActionPreference = 'Stop'
    # Enable native transfer progress only in this script's scope.
    $ProgressPreference = if ($Headless) { 'SilentlyContinue' } else { 'Continue' }
    $progress = @{ Id = 1; Activity = [regex]::Unescape('\u51c6\u5907 ChatGPT \u914d\u7f6e\u5668') }
    function Invoke-SetupProgress([scriptblock]$Action) {
        if ($Headless) { & $Action; return }
        $savedView = $null
        # Minimal progress can count CJK characters as one cell and leave wrapped rows behind.
        if ($PSVersionTable.PSVersion -ge [version]'7.2') {
            $savedView = $PSStyle.Progress.View
            $PSStyle.Progress.View = 'Classic'
        }
        try { & $Action }
        finally {
            try { Write-Progress @progress -Completed }
            finally {
                if ($null -ne $savedView) { $PSStyle.Progress.View = $savedView }
            }
        }
    }
    function Show-SetupStage([string]$Status, [switch]$Completed) {
        if (!$Headless) {
            if ($Completed) { Write-Progress @progress -Completed }
            else { Write-Progress @progress -Status $Status }
        }
    }
    function Get-SetupLocalPath([string]$Path) {
        if ($Path -notmatch '\A[A-Za-z]:\\' -or $Path.IndexOfAny([char[]]'"<>|?*/') -ge 0 -or
            $Path -match '[\x00-\x1f]' -or $Path.Substring(2).Contains(':')) { throw 'invalid-local-path' }
        $full = [IO.Path]::GetFullPath($Path)
        foreach ($component in $full.Substring(3).Split('\')) {
            if ($component -match '[. ]$' -or $component -match '\A(?:CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\.|$)') { throw 'invalid-path-component' }
        }
        $part = $full
        while ($part) {
            if ([IO.File]::Exists($part) -or [IO.Directory]::Exists($part)) {
                if (([IO.File]::GetAttributes($part) -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'redirected-path' }
            }
            $part = [IO.Path]::GetDirectoryName($part)
        }
        return $full
    }
    function Read-SetupKey {
        # Redirected input and non-console hosts may not support keyboard input.
        try {
            if (![Console]::IsInputRedirected -and [Console]::KeyAvailable) {
                return [Console]::ReadKey($true)
            }
        } catch { }
        return $null
    }
    function Show-SetupCompletion {
        Write-Host ''
        # Unicode escapes keep the HTTP entry point ASCII for Windows PowerShell 5.1.
        Write-Host ([regex]::Unescape('\u4e34\u65f6\u6587\u4ef6\u5df2\u6e05\u7406\u3002'))
        Write-Host ([regex]::Unescape('\u559c\u6b22\u672c\u9879\u76ee\u8bf7\u7ed9\u672c\u9879\u76ee\u70b9\u4e00\u4e2a Star \u8c22\u8c22\u55b5~')) -ForegroundColor Cyan
        $url = 'https://github.com/EthenGod/ChatGPT-Proxy-Launcher'
        Write-Host $url
        Write-Host ([regex]::Unescape('\u6309\u4efb\u610f\u952e\u6253\u5f00\u9879\u76ee\u4e3b\u9875\u5e76\u9000\u51fa\uff1b10 \u79d2\u5185\u65e0\u64cd\u4f5c\u5c06\u4f1a\u81ea\u52a8\u6253\u5f00\u5e76\u9000\u51fa\u3002'))
        Write-Host ([regex]::Unescape('\u9000\u51fa\u8bf7\u76f4\u63a5\u5173\u95ed\u6b64\u7a97\u53e3\u3002'))
        for ($tick = 0; $tick -lt 100; $tick++) {
            if ($null -ne (Read-SetupKey)) { break }
            Start-Sleep -Milliseconds 100
        }
        try { Start-Process -FilePath $url }
        catch { Write-Warning "Could not open browser. Open this address manually: $url" }
    }
    $repo = 'EthenGod/ChatGPT-Proxy-Launcher'
    $folder = $null; $exe = $null; $sums = $null; $nativeResult = $null; $resultStream = $null
    $started = $false; $ownedFolder = $false; $cleanupComplete = $true; $phase = 'parse'; $tag = $null
    $outcome = [ordered]@{schemaVersion=1; operation=$Operation; status='error'; exitCode=20; code='invalid-request';
        committed=$false; stage='parse'; nativeError=0; warnings=@(); actionsRequired=@(); data=@{}}
    try {
      if ($Request -or $Result -or $Operation -ne 'deploy') {
        if (!$Headless) { throw 'Parameters require -Headless.' }
      }
      if ($Headless) {
        $Result = Get-SetupLocalPath $Result
        if ([IO.Path]::GetExtension($Result) -ine '.json') { throw 'Result must be a new .json file.' }
        $managedRoot = [IO.Path]::Combine([Environment]::GetFolderPath('LocalApplicationData'), 'ChatGPTProxyLauncher') + '\'
        if ($Result.StartsWith($managedRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Result cannot be inside the managed directory.' }
        if ($Operation -ne 'info') { $Request = Get-SetupLocalPath $Request }
        elseif ($Request) { throw 'info does not accept -Request.' }
        if ($Request -and $Request -ieq $Result) { throw 'Request and Result must differ.' }
        $phase = 'result'
        $resultStream = [IO.File]::Open($Result, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
        $phase = 'transport'
        if ($Request -and (![IO.File]::Exists($Request) -or (Get-Item -LiteralPath $Request).Length -gt 65536)) { throw 'Request is missing or too large.' }
      }
      $tag = Invoke-SetupProgress {
        Show-SetupStage ([regex]::Unescape('\u6b63\u5728\u83b7\u53d6\u6700\u65b0\u7248\u672c'))
        $release = Invoke-RestMethod -Uri "https://api.github.com/repos/$repo/releases/latest" -TimeoutSec 30
        $tag = [string]$release.tag_name
        if ($tag -cnotmatch '\Av[0-9]+(?:\.[0-9]+){2,3}\z') { throw 'Unexpected release tag. Setup was not started.' }
        return $tag
    }
    $base = "https://github.com/$repo/releases/download/$tag"
    $name = 'ChatGPTLauncherSetup.exe'
    $folder = Join-Path $env:TEMP ('ChatGPTLauncherSetup-' + [guid]::NewGuid().ToString('N'))
    $null = New-Item -ItemType Directory -Path $folder
    $ownedFolder = $true
    $sums = Join-Path $folder 'SHA256SUMS.txt'
    $exe = Join-Path $folder $name
        $nativeResult = Join-Path $folder 'native-result.json'
        if (!$Headless) { Write-Host "Downloading ChatGPT Launcher Setup $tag ..." }
        $setupProcess = Invoke-SetupProgress {
            Show-SetupStage ([regex]::Unescape('\u6b63\u5728\u4e0b\u8f7d\u6821\u9a8c\u6587\u4ef6'))
            Invoke-WebRequest -UseBasicParsing -Uri "$base/SHA256SUMS.txt" -OutFile $sums -TimeoutSec 60
            $entries = @([IO.File]::ReadAllLines($sums) -cmatch '\s\*?ChatGPTLauncherSetup\.exe$')
            if ($entries.Count -ne 1 -or $entries[0] -cnotmatch '^([a-fA-F0-9]{64})[ \t]+\*?ChatGPTLauncherSetup\.exe$') {
                throw 'Missing or invalid setup checksum. Setup was not started.'
            }
            $expected = $Matches[1]
            Show-SetupStage ([regex]::Unescape('\u6b63\u5728\u4e0b\u8f7d\u914d\u7f6e\u5668'))
            Invoke-WebRequest -UseBasicParsing -Uri "$base/$name" -OutFile $exe -TimeoutSec 120
            Show-SetupStage ([regex]::Unescape('\u6b63\u5728\u6821\u9a8c\u6587\u4ef6'))
            $stream = [IO.File]::OpenRead($exe)
            try {
                $sha = [Security.Cryptography.SHA256]::Create()
                try { $actual = [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '') }
                finally { $sha.Dispose() }
                if ($actual -ine $expected) { throw 'SHA-256 mismatch. Setup was not started.' }
                Show-SetupStage -Completed
                if ($Headless) {
                    $version = [version](Get-Item -LiteralPath $exe).VersionInfo.FileVersion
                    if ($version -lt [version]'1.0.7') { throw 'Downloaded setup does not support headless deployment.' }
                    $arguments = @('--headless', $Operation, '--result', ('"' + $nativeResult + '"'))
                    if ($Request) { $arguments += @('--request', ('"' + $Request + '"')) }
                    Start-Process -FilePath $exe -WorkingDirectory $folder -ArgumentList $arguments -WindowStyle Hidden -PassThru
                } else { Start-Process -FilePath $exe -WorkingDirectory $folder -PassThru }
            } finally { $stream.Dispose() }
        }
        $started = $true
        try { $setupProcess.WaitForExit(); $nativeExit = $setupProcess.ExitCode } finally { $setupProcess.Dispose() }
        if ($Headless) {
            if (![IO.File]::Exists($nativeResult) -or (Get-Item -LiteralPath $nativeResult).Length -gt 1MB) { throw 'Missing or oversized native result.' }
            $utf8 = New-Object Text.UTF8Encoding($false, $true)
            $native = $utf8.GetString([IO.File]::ReadAllBytes($nativeResult)) | ConvertFrom-Json
            if ($native.schemaVersion -ne 1 -or $native.operation -cne $Operation -or $native.exitCode -ne $nativeExit -or
                $native.status -cnotin @('success','success-with-warnings','error') -or
                $null -eq $native.PSObject.Properties['committed'] -or !$native.code) { throw 'Invalid native result.' }
            if (($null -ne $native.committed -and $native.committed -isnot [bool]) -or
                $native.exitCode -notin @(0,10,20,21,30,31,40,41,50,70) -or
                ($native.exitCode -eq 0 -and $native.status -cne 'success') -or
                ($native.exitCode -eq 10 -and $native.status -cne 'success-with-warnings') -or
                ($native.exitCode -gt 10 -and $native.status -cne 'error')) { throw 'Inconsistent native result.' }
            foreach ($key in @('schemaVersion','operation','status','exitCode','code','committed','stage','nativeError','warnings','actionsRequired','data')) {
                if ($null -eq $native.PSObject.Properties[$key]) { throw 'Incomplete native result.' }
                $outcome[$key] = $native.$key
            }
        }
    } catch {
        if (!$Headless) { throw }
        $outcome.status = 'error'; $outcome.stage = $phase
        $outcome.exitCode = if ($started) { 72 } elseif ($phase -eq 'result') { 70 } elseif ($phase -eq 'parse') { 20 } else { 71 }
        $outcome.code = if ($started) { 'native-result-unavailable' } elseif ($phase -eq 'result') { 'result-unavailable' } elseif ($phase -eq 'parse') { 'invalid-request' } else { 'download-failed' }
        $outcome.committed = if ($started) { $null } else { $false }
    } finally {
        # Only the downloads and headless result are ours. Do not remove other contents.
        if ($ownedFolder -and $folder -and [IO.Directory]::Exists($folder)) {
            if (([IO.File]::GetAttributes($folder) -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                $cleanupComplete = $false
                if (!$Headless) { Write-Warning "Temporary folder was redirected; cleanup skipped: $folder" }
            } else {
                try { [IO.File]::Delete($exe) } catch { $cleanupComplete = $false; if (!$Headless) { Write-Warning "Could not remove temporary file: $exe" } }
                try { [IO.File]::Delete($sums) } catch { $cleanupComplete = $false; if (!$Headless) { Write-Warning "Could not remove temporary file: $sums" } }
                if ($Headless -and $nativeResult) { try { [IO.File]::Delete($nativeResult) } catch { $cleanupComplete = $false } }
                try { [IO.Directory]::Delete($folder, $false) } catch { $cleanupComplete = $false; if (!$Headless) { Write-Warning "Temporary folder retained: $folder" } }
            }
        }
    }
    if ($Headless) {
        $outcome.transport = @{ releaseTag=$tag; cleanupComplete=$cleanupComplete }
        if (!$cleanupComplete) {
            $outcome.warnings = @($outcome.warnings) + @(@{code='cleanup-incomplete'; message='Temporary download files could not be fully removed.'})
            if ($outcome.exitCode -in @(0,10)) { $outcome.exitCode=10; $outcome.status='success-with-warnings'; $outcome.code='cleanup-incomplete' }
        }
        if ($resultStream) {
            try {
                $utf8 = New-Object Text.UTF8Encoding($false)
                $bytes = $utf8.GetBytes(($outcome | ConvertTo-Json -Depth 20))
                $resultStream.Write($bytes, 0, $bytes.Length); $resultStream.Flush($true)
            } catch { $outcome.exitCode=70; $outcome.status='error'; $outcome.code='result-unavailable'; $outcome.stage='result' }
            finally { $resultStream.Dispose() }
        }
        return [pscustomobject]$outcome
    }
    if ($cleanupComplete) {
        Show-SetupCompletion
        # With the documented irm | iex command, exit only this PowerShell session.
        exit 0
    }
}
