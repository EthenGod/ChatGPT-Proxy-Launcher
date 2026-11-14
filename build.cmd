@echo off
rem SPDX-License-Identifier: GPL-3.0-only
setlocal
cd /d "%~dp0"
call tools\msvc-env.cmd
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
cl /nologo /std:c++17 /EHsc /W4 /WX /O1 /GL /MT /utf-8 /DUNICODE /D_UNICODE /Fo:work\ /Fe:outputs\ChatGPTProxyLauncher.exe src\ChatGPTProxyLauncher.cpp src\TaskbarShortcut.cpp src\StartupApps.cpp src\AutomationJson.cpp src\AutomationIO.cpp src\DeploymentDiscovery.cpp src\CompanionManifest.cpp src\CompanionProtocol.cpp src\CompanionRunner.cpp src\LauncherAutomation.cpp work\LauncherResources.res /link /LTCG /MANIFEST:EMBED /MANIFESTINPUT:src\Launcher.manifest /SUBSYSTEM:WINDOWS /OPT:REF /OPT:ICF /DYNAMICBASE /NXCOMPAT kernel32.lib shell32.lib ole32.lib uuid.lib advapi32.lib bcrypt.lib version.lib comctl32.lib user32.lib
if errorlevel 1 exit /b %errorlevel%
pushd src
rc /nologo /fo ..\work\SetupResources.res SetupResources.rc
if errorlevel 1 (
    popd
    exit /b 1
)
popd
cl /nologo /std:c++17 /W4 /WX /O1 /GL /MT /EHsc /utf-8 /DUNICODE /D_UNICODE /Fo:work\ /Fe:outputs\ChatGPTLauncherSetup.exe src\ChatGPTLauncherSetup.cpp src\SetupCore.cpp src\SetupArguments.cpp src\CodexEnv.cpp src\StartupIntegration.cpp src\ProxyDetection.cpp src\CodexClose.cpp src\AutomationJson.cpp src\AutomationIO.cpp src\DeploymentDiscovery.cpp src\SetupCommandLine.cpp src\CompanionManifest.cpp src\CompanionRegistry.cpp work\SetupResources.res /link /LTCG /MANIFEST:NO /SUBSYSTEM:WINDOWS /OPT:REF /OPT:ICF /DYNAMICBASE /NXCOMPAT kernel32.lib user32.lib gdi32.lib gdiplus.lib shell32.lib ole32.lib uuid.lib shlwapi.lib comctl32.lib uxtheme.lib dwmapi.lib advapi32.lib xmllite.lib iphlpapi.lib ws2_32.lib runtimeobject.lib version.lib
exit /b %errorlevel%
