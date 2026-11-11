@echo off
rem SPDX-License-Identifier: GPL-3.0-only
setlocal
cd /d "%~dp0.."
call tools\msvc-env.cmd
if errorlevel 1 exit /b %errorlevel%
if not exist work\analysis mkdir work\analysis
cl /nologo /std:c++17 /EHsc /W4 /WX /analyze /analyze:external- /external:anglebrackets /external:W0 /c /MT /utf-8 /DUNICODE /D_UNICODE /Fo:work\analysis\ src\ChatGPTProxyLauncher.cpp src\TaskbarShortcut.cpp src\StartupApps.cpp src\ChatGPTLauncherSetup.cpp src\SetupCore.cpp src\StartupIntegration.cpp src\ProxyDetection.cpp src\CodexClose.cpp
exit /b %errorlevel%
