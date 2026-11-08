@echo off
rem SPDX-License-Identifier: GPL-3.0-only
setlocal
cd /d "%~dp0.."
call build.cmd
if errorlevel 1 exit /b %errorlevel%
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /W4 /WX /O1 /MT /EHsc /utf-8 /DUNICODE /D_UNICODE /Fo:work\launcher-tests.obj /Fe:work\launcher-tests.exe tests\launcher-tests.cpp /link /SUBSYSTEM:CONSOLE kernel32.lib
if errorlevel 1 exit /b %errorlevel%
work\launcher-tests.exe "%CD%\work\capture with spaces.txt"
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /W4 /WX /O1 /MT /EHsc /utf-8 /DUNICODE /D_UNICODE /Fo:work\companion-tests.obj /Fe:work\companion-tests.exe tests\companion-tests.cpp work\SetupCore.obj /link /LTCG /SUBSYSTEM:CONSOLE kernel32.lib shell32.lib ole32.lib uuid.lib shlwapi.lib version.lib
if errorlevel 1 exit /b %errorlevel%
work\companion-tests.exe "%CD%\work"
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /W4 /WX /O1 /MT /EHsc /utf-8 /DUNICODE /D_UNICODE /Fo:work\taskbar-tests.obj /Fe:work\taskbar-tests.exe tests\taskbar-tests.cpp /link /SUBSYSTEM:CONSOLE kernel32.lib shell32.lib ole32.lib uuid.lib
if errorlevel 1 exit /b %errorlevel%
work\taskbar-tests.exe "%CD%\work" "%CD%\outputs\ChatGPTProxyLauncher.exe"
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /W4 /WX /O1 /MT /EHsc /utf-8 /DUNICODE /D_UNICODE /Fo:work\setup-tests.obj /Fe:work\setup-tests.exe tests\setup-tests.cpp /link /SUBSYSTEM:CONSOLE kernel32.lib shell32.lib ole32.lib uuid.lib shlwapi.lib version.lib
if errorlevel 1 exit /b %errorlevel%
work\setup-tests.exe "%CD%\work" "%CD%\outputs\ChatGPTProxyLauncher.exe" "%CD%\outputs\ChatGPTLauncherSetup.exe"
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /W4 /WX /O1 /MT /EHsc /utf-8 /DUNICODE /D_UNICODE /Fo:work\startup-tests.obj /Fe:work\startup-tests.exe tests\startup-tests.cpp work\SetupCore.obj /link /LTCG /SUBSYSTEM:CONSOLE kernel32.lib shell32.lib ole32.lib uuid.lib shlwapi.lib advapi32.lib xmllite.lib version.lib
if errorlevel 1 exit /b %errorlevel%
work\startup-tests.exe
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /W4 /WX /O1 /MT /EHsc /utf-8 /DUNICODE /D_UNICODE /Fo:work\proxy-tests.obj /Fe:work\proxy-tests.exe tests\proxy-tests.cpp /link /SUBSYSTEM:CONSOLE kernel32.lib shell32.lib ole32.lib uuid.lib advapi32.lib iphlpapi.lib ws2_32.lib runtimeobject.lib
if errorlevel 1 exit /b %errorlevel%
work\proxy-tests.exe
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /W4 /WX /O1 /MT /EHsc /utf-8 /DUNICODE /D_UNICODE /Fo:work\codex-close-tests.obj /Fe:work\codex-close-tests.exe tests\codex-close-tests.cpp /link /SUBSYSTEM:CONSOLE kernel32.lib user32.lib advapi32.lib
if errorlevel 1 exit /b %errorlevel%
work\codex-close-tests.exe
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /W4 /WX /O1 /MT /EHsc /utf-8 /DUNICODE /D_UNICODE /Fo:work\setup-ui-tests.obj /Fe:work\setup-ui-tests.exe tests\setup-ui-tests.cpp work\SetupCore.obj /link /LTCG /SUBSYSTEM:CONSOLE /MANIFEST:EMBED /MANIFESTINPUT:src\Setup.manifest kernel32.lib user32.lib gdi32.lib gdiplus.lib shell32.lib ole32.lib uuid.lib shlwapi.lib comctl32.lib uxtheme.lib dwmapi.lib advapi32.lib xmllite.lib version.lib
if errorlevel 1 exit /b %errorlevel%
work\setup-ui-tests.exe "%CD%\work"
if errorlevel 1 exit /b %errorlevel%
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0download-tests.ps1"
exit /b %errorlevel%
