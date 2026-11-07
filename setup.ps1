# SPDX-License-Identifier: GPL-3.0-only
# Keep this entry point ASCII so Windows PowerShell can execute it from an HTTP response.
& {
    $ErrorActionPreference = 'Stop'
    # Enable native transfer progress only in this script's scope.
    $ProgressPreference = 'Continue'
    $progress = @{ Id = 1; Activity = [regex]::Unescape('\u51c6\u5907 ChatGPT \u914d\u7f6e\u5668') }
    function Invoke-SetupProgress([scriptblock]$Action) {
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
    $tag = Invoke-SetupProgress {
        Write-Progress @progress -Status ([regex]::Unescape('\u6b63\u5728\u83b7\u53d6\u6700\u65b0\u7248\u672c'))
        $release = Invoke-RestMethod -Uri "https://api.github.com/repos/$repo/releases/latest" -TimeoutSec 30
        $tag = [string]$release.tag_name
        if ($tag -cnotmatch '\Av[0-9]+(?:\.[0-9]+){2,3}\z') { throw 'Unexpected release tag. Setup was not started.' }
        return $tag
    }
    $base = "https://github.com/$repo/releases/download/$tag"
    $name = 'ChatGPTLauncherSetup.exe'
    $folder = Join-Path $env:TEMP ('ChatGPTLauncherSetup-' + [guid]::NewGuid().ToString('N'))
    $null = New-Item -ItemType Directory -Path $folder
    $sums = Join-Path $folder 'SHA256SUMS.txt'
    $exe = Join-Path $folder $name
    $cleanupComplete = $true
    try {
        Write-Host "Downloading ChatGPT Launcher Setup $tag ..."
        $setupProcess = Invoke-SetupProgress {
            Write-Progress @progress -Status ([regex]::Unescape('\u6b63\u5728\u4e0b\u8f7d\u6821\u9a8c\u6587\u4ef6'))
            Invoke-WebRequest -UseBasicParsing -Uri "$base/SHA256SUMS.txt" -OutFile $sums -TimeoutSec 60
            $entries = @([IO.File]::ReadAllLines($sums) -cmatch '\s\*?ChatGPTLauncherSetup\.exe$')
            if ($entries.Count -ne 1 -or $entries[0] -cnotmatch '^([a-fA-F0-9]{64})[ \t]+\*?ChatGPTLauncherSetup\.exe$') {
                throw 'Missing or invalid setup checksum. Setup was not started.'
            }
            $expected = $Matches[1]
            Write-Progress @progress -Status ([regex]::Unescape('\u6b63\u5728\u4e0b\u8f7d\u914d\u7f6e\u5668'))
            Invoke-WebRequest -UseBasicParsing -Uri "$base/$name" -OutFile $exe -TimeoutSec 120
            Write-Progress @progress -Status ([regex]::Unescape('\u6b63\u5728\u6821\u9a8c\u6587\u4ef6'))
            $stream = [IO.File]::OpenRead($exe)
            try {
                $sha = [Security.Cryptography.SHA256]::Create()
                try { $actual = [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '') }
                finally { $sha.Dispose() }
                if ($actual -ine $expected) { throw 'SHA-256 mismatch. Setup was not started.' }
                Write-Progress @progress -Completed
                Start-Process -FilePath $exe -WorkingDirectory $folder -PassThru
            } finally { $stream.Dispose() }
        }
        try { $setupProcess.WaitForExit() } finally { $setupProcess.Dispose() }
    } finally {
        # Only the two download paths are ours. Never follow a redirected folder or remove its other contents.
        if ([IO.Directory]::Exists($folder)) {
            if (([IO.File]::GetAttributes($folder) -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                $cleanupComplete = $false
                Write-Warning "Temporary folder was redirected; cleanup skipped: $folder"
            } else {
                try { [IO.File]::Delete($exe) } catch { $cleanupComplete = $false; Write-Warning "Could not remove temporary file: $exe" }
                try { [IO.File]::Delete($sums) } catch { $cleanupComplete = $false; Write-Warning "Could not remove temporary file: $sums" }
                try { [IO.Directory]::Delete($folder, $false) } catch { $cleanupComplete = $false; Write-Warning "Temporary folder retained: $folder" }
            }
        }
    }
    if ($cleanupComplete) {
        Show-SetupCompletion
        # With the documented irm | iex command, exit only this PowerShell session.
        exit 0
    }
}
