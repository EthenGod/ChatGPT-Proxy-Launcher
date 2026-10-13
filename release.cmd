@echo off
rem SPDX-License-Identifier: GPL-3.0-only
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0release.ps1"
exit /b %errorlevel%
