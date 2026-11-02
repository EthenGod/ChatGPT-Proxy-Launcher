# SPDX-License-Identifier: GPL-3.0-only
# Keep this entry point ASCII so Windows PowerShell can execute it from an HTTP response.
& {
    $ErrorActionPreference = 'Stop'
    $repo = 'EthenGod/ChatGPT-Proxy-Launcher'
    $release = Invoke-RestMethod -Uri "https://api.github.com/repos/$repo/releases/latest" -TimeoutSec 30
    $tag = [string]$release.tag_name
    if ($tag -cnotmatch '\Av[0-9]+(?:\.[0-9]+){2,3}\z') { throw 'Unexpected release tag. Setup was not started.' }
    $base = "https://github.com/$repo/releases/download/$tag"
    $name = 'ChatGPTLauncherSetup.exe'
    $folder = Join-Path $env:TEMP ('ChatGPTLauncherSetup-' + [guid]::NewGuid().ToString('N'))
    $null = New-Item -ItemType Directory -Path $folder
    $sums = Join-Path $folder 'SHA256SUMS.txt'
    $exe = Join-Path $folder $name
    Write-Host "Downloading ChatGPT Launcher Setup $tag ..."
    Invoke-WebRequest -UseBasicParsing -Uri "$base/SHA256SUMS.txt" -OutFile $sums -TimeoutSec 60
    $entries = @([IO.File]::ReadAllLines($sums) -cmatch '\s\*?ChatGPTLauncherSetup\.exe$')
    if ($entries.Count -ne 1 -or $entries[0] -cnotmatch '^([a-fA-F0-9]{64})[ \t]+\*?ChatGPTLauncherSetup\.exe$') {
        throw 'Missing or invalid setup checksum. Setup was not started.'
    }
    $expected = $Matches[1]
    Invoke-WebRequest -UseBasicParsing -Uri "$base/$name" -OutFile $exe -TimeoutSec 120
    $stream = [IO.File]::OpenRead($exe)
    try {
        $sha = [Security.Cryptography.SHA256]::Create()
        try { $actual = [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '') }
        finally { $sha.Dispose() }
        if ($actual -ine $expected) { throw 'SHA-256 mismatch. Setup was not started.' }
        Start-Process -FilePath $exe -WorkingDirectory $folder
    } finally { $stream.Dispose() }
}
