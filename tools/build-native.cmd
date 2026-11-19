@echo off
rem SPDX-License-Identifier: GPL-3.0-only
setlocal
set "PSModulePath=%SystemRoot%\System32\WindowsPowerShell\v1.0\Modules"
cd /d "%~dp0.."
if not "%~1"=="" if /i not "%~1"=="launcher" if /i not "%~1"=="test-launcher" if /i not "%~1"=="native" exit /b 2
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File tools\locales.ps1 -Generate
if errorlevel 1 exit /b %errorlevel%
call tools\msvc-env.cmd
if errorlevel 1 exit /b %errorlevel%
set "launcherDir=outputs"
set "objectDir=work"
set "setupRcFlags="
if /i "%~1"=="native" (
    set "launcherDir=work\ui-native"
    set "objectDir=work\ui-native"
    set "setupRcFlags=/d SETUP_STANDALONE_NATIVE"
)
if /i "%~1"=="test-launcher" (
    set "launcherDir=work\ui-test-native"
    set "objectDir=work\ui-test-native"
)
if not exist work mkdir work
if not exist "%launcherDir%" mkdir "%launcherDir%"
pushd src
rc /nologo /fo ..\%objectDir%\LauncherResources.res LauncherResources.rc
if errorlevel 1 (
    popd
    exit /b 1
)
popd
cl /nologo @tools\msvc-common.rsp /DCHATGPT_LAUNCHER_ONLY /O1 /GL /MT /Fo:%objectDir%\ /Fe:%launcherDir%\ChatGPTProxyLauncher.exe src\Localization.cpp src\ChatGPTProxyLauncher.cpp src\TaskbarShortcut.cpp src\StartupApps.cpp src\AutomationJson.cpp src\AutomationIO.cpp src\DeploymentDiscovery.cpp src\CompanionManifest.cpp src\CompanionProtocol.cpp src\CompanionRunner.cpp src\LauncherAutomation.cpp %objectDir%\LauncherResources.res /link /LTCG /MANIFEST:EMBED /MANIFESTINPUT:src\Launcher.manifest /SUBSYSTEM:WINDOWS /OPT:REF /OPT:ICF /DYNAMICBASE /NXCOMPAT kernel32.lib shell32.lib ole32.lib uuid.lib advapi32.lib bcrypt.lib version.lib comctl32.lib user32.lib
if errorlevel 1 exit /b %errorlevel%
if /i "%~1"=="launcher" exit /b 0
if /i "%~1"=="test-launcher" exit /b 0
if not exist work\ui-native mkdir work\ui-native
pushd src
rc /nologo %setupRcFlags% /fo ..\%objectDir%\SetupResources.res SetupResources.rc
if errorlevel 1 (
    popd
    exit /b 1
)
popd
cl /nologo @tools\msvc-common.rsp /O1 /GL /MT /Fo:%objectDir%\ /Fe:work\ui-native\ChatGPTLauncherSetup.exe src\Localization.cpp src\ChatGPTLauncherSetup.cpp src\SetupUiServices.cpp src\SetupCore.cpp src\SetupArguments.cpp src\CodexEnv.cpp src\StartupIntegration.cpp src\ProxyDetection.cpp src\CodexClose.cpp src\AutomationJson.cpp src\AutomationIO.cpp src\DeploymentDiscovery.cpp src\SetupCommandLine.cpp src\CompanionManifest.cpp src\CompanionRegistry.cpp %objectDir%\SetupResources.res /link /LTCG /MANIFEST:NO /SUBSYSTEM:WINDOWS /OPT:REF /OPT:ICF /DYNAMICBASE /NXCOMPAT kernel32.lib user32.lib gdi32.lib gdiplus.lib shell32.lib ole32.lib uuid.lib shlwapi.lib comctl32.lib uxtheme.lib dwmapi.lib advapi32.lib xmllite.lib iphlpapi.lib ws2_32.lib runtimeobject.lib version.lib
exit /b %errorlevel%
