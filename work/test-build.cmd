@echo off
rem SPDX-License-Identifier: GPL-3.0-only
setlocal
cd /d "%~dp0.."
call build.cmd
if errorlevel 1 exit /b %errorlevel%
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
cl /nologo /W4 /WX /O1 /MT /EHsc /utf-8 /DUNICODE /D_UNICODE /Fo:work\launcher-tests.obj /Fe:work\launcher-tests.exe work\launcher-tests.cpp /link /SUBSYSTEM:CONSOLE kernel32.lib
if errorlevel 1 exit /b %errorlevel%
work\launcher-tests.exe "%CD%\work\capture with spaces.txt"
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /W4 /WX /O1 /MT /EHsc /utf-8 /DUNICODE /D_UNICODE /Fo:work\setup-tests.obj /Fe:work\setup-tests.exe tests\setup-tests.cpp /link /SUBSYSTEM:CONSOLE kernel32.lib shell32.lib ole32.lib uuid.lib shlwapi.lib
if errorlevel 1 exit /b %errorlevel%
work\setup-tests.exe "%CD%\work" "%CD%\outputs\ChatGPTProxyLauncher.exe" "%CD%\outputs\ChatGPTLauncherSetup.exe"
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /W4 /WX /O1 /MT /EHsc /utf-8 /DUNICODE /D_UNICODE /Fo:work\startup-tests.obj /Fe:work\startup-tests.exe tests\startup-tests.cpp work\SetupCore.obj /link /LTCG /SUBSYSTEM:CONSOLE kernel32.lib shell32.lib ole32.lib uuid.lib shlwapi.lib advapi32.lib xmllite.lib
if errorlevel 1 exit /b %errorlevel%
work\startup-tests.exe
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /W4 /WX /O1 /MT /EHsc /utf-8 /DUNICODE /D_UNICODE /Fo:work\proxy-tests.obj /Fe:work\proxy-tests.exe tests\proxy-tests.cpp /link /SUBSYSTEM:CONSOLE kernel32.lib shell32.lib ole32.lib uuid.lib advapi32.lib iphlpapi.lib ws2_32.lib runtimeobject.lib
if errorlevel 1 exit /b %errorlevel%
work\proxy-tests.exe
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /W4 /WX /O1 /MT /EHsc /utf-8 /DUNICODE /D_UNICODE /Fo:work\setup-ui-tests.obj /Fe:work\setup-ui-tests.exe tests\setup-ui-tests.cpp work\SetupCore.obj /link /LTCG /SUBSYSTEM:CONSOLE kernel32.lib user32.lib gdi32.lib gdiplus.lib shell32.lib ole32.lib uuid.lib shlwapi.lib comctl32.lib uxtheme.lib dwmapi.lib advapi32.lib xmllite.lib
if errorlevel 1 exit /b %errorlevel%
work\setup-ui-tests.exe "%CD%\work"
exit /b %errorlevel%
