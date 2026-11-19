# SPDX-License-Identifier: GPL-3.0-only
param([switch]$Probe)
$ErrorActionPreference = 'Stop'
if ($Probe) {
    # A different process owns this handle while the public build runs its native or embedding step.
    $path = Join-Path $PSScriptRoot 'work\ui-target\desktop-build.lock'
    try { $free = [IO.File]::Open($path, 'Open', 'ReadWrite', 'None') }
    catch [IO.IOException] {
        if (($_.Exception.GetBaseException().HResult -band 65535) -eq 32) { return }
        throw
    }
    $free.Dispose()
    throw 'Build entered a write or embedding step without the shared lock'
}

$project = Split-Path $PSScriptRoot
$fixture = Join-Path $project ('work\build-lock-' + [guid]::NewGuid().ToString('N'))
foreach ($dir in 'tools','frontend\node_modules\typescript','work\ui-toolchain\bin','work\ui-target\release','outputs') {
    $null = [IO.Directory]::CreateDirectory((Join-Path $fixture $dir))
}
foreach ($file in 'build.cmd','frontend\desktop.ps1') {
    Copy-Item -LiteralPath (Join-Path $project $file) -Destination (Join-Path $fixture $file)
}
Copy-Item -LiteralPath $PSCommandPath -Destination (Join-Path $fixture 'probe.ps1')
# The public scripts run normally. Stand-ins check the lock instead of compiling.
foreach ($file in 'work\ui-toolchain\bin\cargo.exe','outputs\ChatGPTProxyLauncher.exe','work\LocalizationData.h','work\ui-target\release\ChatGPTLauncherSetup.exe') {
    [IO.File]::WriteAllText((Join-Path $fixture $file), 'fixture')
}
$probeCommand = '@powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0..\probe.ps1" -Probe'
[IO.File]::WriteAllText((Join-Path $fixture 'tools\build-native.cmd'), $probeCommand)
[IO.File]::WriteAllText((Join-Path $fixture 'work\ui-toolchain\bin\npm.cmd'), $probeCommand.Replace('..\probe.ps1','..\..\..\probe.ps1'))
Push-Location $fixture
try {
    & .\build.cmd launcher
    if ($LASTEXITCODE) { throw 'Standalone launcher lock check failed' }
    & .\build.cmd native
    if ($LASTEXITCODE) { throw 'Standalone compatibility lock check failed' }
    & .\build.cmd
    if ($LASTEXITCODE -or ![IO.File]::Exists((Join-Path $fixture 'work\ui-build.json'))) { throw 'Complete build lock check failed' }
} finally { Pop-Location }
Write-Host 'PASS standalone and complete builds hold the same exclusive file lock during compilation and embedding'
