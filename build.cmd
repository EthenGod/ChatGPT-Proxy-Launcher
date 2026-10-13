@echo off
rem SPDX-License-Identifier: GPL-3.0-only
setlocal
cd /d "%~dp0"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
if not exist outputs mkdir outputs
if not exist work mkdir work
pushd src
rc /nologo /fo ..\work\LauncherResources.res LauncherResources.rc
if errorlevel 1 (
    popd
    exit /b 1
)
popd
cl /nologo /W4 /WX /O1 /GL /MT /utf-8 /DUNICODE /D_UNICODE /Fo:work\ChatGPTProxyLauncher.obj /Fe:outputs\ChatGPTProxyLauncher.exe src\ChatGPTProxyLauncher.cpp work\LauncherResources.res /link /LTCG /SUBSYSTEM:WINDOWS /OPT:REF /OPT:ICF /DYNAMICBASE /NXCOMPAT kernel32.lib
if errorlevel 1 exit /b %errorlevel%
pushd src
rc /nologo /fo ..\work\SetupResources.res SetupResources.rc
if errorlevel 1 (
    popd
    exit /b 1
)
popd
cl /nologo /std:c++17 /W4 /WX /O1 /GL /MT /EHsc /utf-8 /DUNICODE /D_UNICODE /Fo:work\ /Fe:outputs\ChatGPTLauncherSetup.exe src\ChatGPTLauncherSetup.cpp src\SetupCore.cpp src\StartupIntegration.cpp src\ProxyDetection.cpp work\SetupResources.res /link /LTCG /MANIFEST:NO /SUBSYSTEM:WINDOWS /OPT:REF /OPT:ICF /DYNAMICBASE /NXCOMPAT kernel32.lib user32.lib gdi32.lib gdiplus.lib shell32.lib ole32.lib uuid.lib shlwapi.lib comctl32.lib uxtheme.lib dwmapi.lib advapi32.lib xmllite.lib iphlpapi.lib ws2_32.lib runtimeobject.lib
exit /b %errorlevel%
