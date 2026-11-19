@echo off
rem SPDX-License-Identifier: GPL-3.0-only
setlocal
rem Windows PowerShell must not inherit PowerShell 7-only module paths.
set "PSModulePath=%SystemRoot%\System32\WindowsPowerShell\v1.0\Modules"
cd /d "%~dp0"
if /i "%~1"=="native" goto native
if /i "%~1"=="launcher" goto launcher
if not "%~1"=="" (
    echo Usage: build.cmd [native^|launcher]
    exit /b 2
)
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File frontend\desktop.ps1 -Command build
exit /b %errorlevel%
:native
call tools\build-native.cmd native
exit /b %errorlevel%
:launcher
call tools\build-native.cmd launcher
exit /b %errorlevel%
