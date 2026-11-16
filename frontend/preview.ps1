# SPDX-License-Identifier: GPL-3.0-only
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot
$exe = Join-Path $projectRoot 'work\ui-prototype\ChatGPTLauncherSetup.exe'
if (![IO.File]::Exists($exe)) { throw 'Build first: frontend\desktop.ps1 -Command prototype' }
$session = Join-Path $projectRoot ('work\ui-prototype-sessions\' + [guid]::NewGuid().ToString('N'))
$null = [IO.Directory]::CreateDirectory($session)
$info = New-Object Diagnostics.ProcessStartInfo
$info.FileName = $exe
$info.WorkingDirectory = $session
$info.UseShellExecute = $false
$info.EnvironmentVariables['CHATGPT_SETUP_TEST_ROOT'] = $session
$process = [Diagnostics.Process]::Start($info)
try { $process.WaitForExit(); if ($process.ExitCode -ne 0) { throw "Preview exited: $($process.ExitCode)" } }
finally { $process.Dispose() }
