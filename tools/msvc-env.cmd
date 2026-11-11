@echo off
rem SPDX-License-Identifier: GPL-3.0-only
rem Deliberately no setlocal: the caller needs the compiler environment.
if /i "%VSCMD_ARG_TGT_ARCH%"=="x64" (
    where cl.exe >nul 2>nul
    if not errorlevel 1 exit /b 0
)
set "CHATGPT_VS_ROOT="
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
    for /f "usebackq delims=" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "CHATGPT_VS_ROOT=%%I"
)
if not defined CHATGPT_VS_ROOT set "CHATGPT_VS_ROOT=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools"
if not exist "%CHATGPT_VS_ROOT%\Common7\Tools\VsDevCmd.bat" (
    echo Install Visual Studio Build Tools with Desktop development with C++ and a Windows SDK. 1>&2
    exit /b 1
)
call "%CHATGPT_VS_ROOT%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
exit /b %errorlevel%
