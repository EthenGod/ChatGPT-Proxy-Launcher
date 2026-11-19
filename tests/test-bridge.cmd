@echo off
rem SPDX-License-Identifier: GPL-3.0-only
setlocal
cd /d "%~dp0.."
set "nativeObjects=work"
set "launcher=outputs\ChatGPTProxyLauncher.exe"
if /i "%~1"=="--reuse-build" goto native_ready
if not "%~1"=="" exit /b 2
call build.cmd native
if errorlevel 1 exit /b %errorlevel%
set "nativeObjects=work\ui-native"
set "launcher=work\ui-native\ChatGPTProxyLauncher.exe"
:native_ready
call tools\msvc-env.cmd
if errorlevel 1 exit /b %errorlevel%
cl /nologo @tools\msvc-common.rsp /O1 /MT /c /Fo:work\ src\SetupBridge.cpp src\SetupProgramIcons.cpp
if errorlevel 1 exit /b %errorlevel%
cl /nologo @tools\msvc-common.rsp /O1 /MT /DSETUP_UI_EMBEDDED /c /Fo:work\SetupNative.obj src\ChatGPTLauncherSetup.cpp
if errorlevel 1 exit /b %errorlevel%
set "BRIDGE_OBJECTS=work\SetupProgramIcons.obj work\SetupNative.obj %nativeObjects%\SetupUiServices.obj %nativeObjects%\ProxyDetection.obj %nativeObjects%\CodexClose.obj %nativeObjects%\SetupCore.obj %nativeObjects%\SetupArguments.obj %nativeObjects%\CodexEnv.obj %nativeObjects%\AutomationJson.obj %nativeObjects%\AutomationIO.obj %nativeObjects%\DeploymentDiscovery.obj %nativeObjects%\SetupCommandLine.obj %nativeObjects%\CompanionManifest.obj %nativeObjects%\CompanionRegistry.obj %nativeObjects%\StartupIntegration.obj %nativeObjects%\Localization.obj %nativeObjects%\SetupResources.res"
set "BRIDGE_LIBS=kernel32.lib user32.lib shell32.lib ole32.lib uuid.lib shlwapi.lib windowscodecs.lib crypt32.lib advapi32.lib xmllite.lib version.lib gdi32.lib gdiplus.lib comctl32.lib uxtheme.lib dwmapi.lib iphlpapi.lib ws2_32.lib runtimeobject.lib"
cl /nologo @tools\msvc-common.rsp /O1 /MT /Fo:work\setup-bridge-tests.obj /Fe:work\setup-bridge-tests.exe tests\setup-bridge-tests.cpp work\SetupBridge.obj %BRIDGE_OBJECTS% /link /LTCG /MANIFEST:NO /SUBSYSTEM:CONSOLE %BRIDGE_LIBS%
if errorlevel 1 exit /b %errorlevel%
work\setup-bridge-tests.exe "%CD%\work" "%CD%\%launcher%"
if errorlevel 1 exit /b %errorlevel%
cl /nologo @tools\msvc-common.rsp /O1 /MT /Fo:work\setup-bridge-actions-tests.obj /Fe:work\setup-bridge-actions-tests.exe tests\setup-bridge-actions-tests.cpp %BRIDGE_OBJECTS% /link /LTCG /MANIFEST:NO /SUBSYSTEM:CONSOLE %BRIDGE_LIBS%
if errorlevel 1 exit /b %errorlevel%
work\setup-bridge-actions-tests.exe "%CD%\work" "%CD%\%launcher%"
if errorlevel 1 exit /b %errorlevel%
cl /nologo @tools\msvc-common.rsp /O1 /MT /Fo:work\setup-bridge-runner.obj /Fe:work\setup-bridge-runner.exe tests\setup-bridge-runner.cpp %BRIDGE_OBJECTS% /link /LTCG /MANIFEST:NO /SUBSYSTEM:WINDOWS %BRIDGE_LIBS%
exit /b %errorlevel%
