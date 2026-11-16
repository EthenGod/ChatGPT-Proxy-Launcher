@echo off
rem SPDX-License-Identifier: GPL-3.0-only
setlocal
cd /d "%~dp0.."
call build.cmd
if errorlevel 1 exit /b %errorlevel%
call tools\msvc-env.cmd
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /W4 /WX /O1 /MT /utf-8 /DUNICODE /D_UNICODE /c /Fo:work\SetupBridge.obj src\SetupBridge.cpp
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /W4 /WX /O1 /MT /utf-8 /DUNICODE /D_UNICODE /DSETUP_UI_EMBEDDED /c /Fo:work\SetupNative.obj src\ChatGPTLauncherSetup.cpp
if errorlevel 1 exit /b %errorlevel%
set "BRIDGE_OBJECTS=work\SetupNative.obj work\ProxyDetection.obj work\CodexClose.obj work\SetupCore.obj work\SetupArguments.obj work\CodexEnv.obj work\AutomationJson.obj work\AutomationIO.obj work\DeploymentDiscovery.obj work\SetupCommandLine.obj work\CompanionManifest.obj work\CompanionRegistry.obj work\StartupIntegration.obj work\Localization.obj work\SetupResources.res"
set "BRIDGE_LIBS=kernel32.lib user32.lib shell32.lib ole32.lib uuid.lib shlwapi.lib advapi32.lib xmllite.lib version.lib gdi32.lib gdiplus.lib comctl32.lib uxtheme.lib dwmapi.lib iphlpapi.lib ws2_32.lib runtimeobject.lib"
cl /nologo /std:c++17 /EHsc /W4 /WX /O1 /MT /utf-8 /DUNICODE /D_UNICODE /Fo:work\setup-bridge-tests.obj /Fe:work\setup-bridge-tests.exe tests\setup-bridge-tests.cpp work\SetupBridge.obj %BRIDGE_OBJECTS% /link /LTCG /MANIFEST:NO /SUBSYSTEM:CONSOLE %BRIDGE_LIBS%
if errorlevel 1 exit /b %errorlevel%
work\setup-bridge-tests.exe "%CD%\work" "%CD%\outputs\ChatGPTProxyLauncher.exe"
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /W4 /WX /O1 /MT /utf-8 /DUNICODE /D_UNICODE /Fo:work\setup-bridge-runner.obj /Fe:work\setup-bridge-runner.exe tests\setup-bridge-runner.cpp %BRIDGE_OBJECTS% /link /LTCG /MANIFEST:NO /SUBSYSTEM:WINDOWS %BRIDGE_LIBS%
exit /b %errorlevel%
