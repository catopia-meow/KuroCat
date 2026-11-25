@echo off
setlocal
cd /d "%~dp0"

echo -- Building SachiCat.exe (Windows native C++17 / Win32 + GDI+ + Raw Input)...

if not exist build mkdir build

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0write-version.ps1" -OutputDirectory build
if errorlevel 1 exit /b 1

where cl >nul 2>nul
if %ERRORLEVEL%==0 (
    rc /nologo /I build /fo build\SachiCat-version.res version.rc
    if errorlevel 1 exit /b 1
    cl /nologo /Ibuild /std:c++17 /O2 /MT /EHsc /utf-8 /DUNICODE /D_UNICODE ^
       /W3 src\main.cpp build\SachiCat-version.res /Fe:SachiCat.exe ^
       /link /SUBSYSTEM:WINDOWS /MANIFEST:EMBED /MANIFESTINPUT:build\SachiCat.manifest ^
       user32.lib gdi32.lib gdiplus.lib shell32.lib advapi32.lib wtsapi32.lib ole32.lib uuid.lib
    if errorlevel 1 exit /b 1
    del /q main.obj 2>nul
    echo -- Built successfully with MSVC: %~dp0SachiCat.exe
    exit /b 0
)

where g++ >nul 2>nul
if %ERRORLEVEL%==0 (
    windres -I build -DSACHICAT_EMBED_MANIFEST --input version.rc --output-format=coff --output build\SachiCat-version.o
    if errorlevel 1 exit /b 1
    g++ -Ibuild -std=c++17 -O2 -mwindows -municode -DUNICODE -D_UNICODE -static ^
        src\main.cpp build\SachiCat-version.o -o SachiCat.exe ^
        -luser32 -lgdi32 -lgdiplus -lshell32 -ladvapi32 -lwtsapi32 -lole32 -luuid
    if errorlevel 1 exit /b 1
    echo -- Built successfully with MinGW: %~dp0SachiCat.exe
    exit /b 0
)

echo [ERROR] Neither MSVC (cl.exe) nor MinGW (g++.exe) found in PATH.
echo Please run from "x64 Native Tools Command Prompt for VS" or install Visual Studio Build Tools.
exit /b 1
