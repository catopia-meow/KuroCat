@echo off
setlocal
cd /d "%~dp0"

echo -- Building KuroCat.exe (Windows native C++17 / Win32 + GDI+ + Raw Input)...

if not exist build mkdir build

where cl >nul 2>nul
if %ERRORLEVEL%==0 (
    rc /nologo /fo build\KuroCat-version.res version.rc
    if errorlevel 1 exit /b 1
    cl /nologo /std:c++17 /O2 /MT /EHsc /utf-8 /DUNICODE /D_UNICODE ^
       /W3 src\main.cpp build\KuroCat-version.res /Fe:KuroCat.exe ^
       /link /SUBSYSTEM:WINDOWS /MANIFEST:EMBED /MANIFESTINPUT:app.manifest ^
       user32.lib gdi32.lib gdiplus.lib shell32.lib advapi32.lib winhttp.lib wtsapi32.lib ole32.lib uuid.lib
    if errorlevel 1 exit /b 1
    del /q main.obj 2>nul
    echo -- Built successfully with MSVC: %~dp0KuroCat.exe
    exit /b 0
)

where g++ >nul 2>nul
if %ERRORLEVEL%==0 (
    windres --input version.rc --output-format=coff --output build\KuroCat-version.o
    if errorlevel 1 exit /b 1
    g++ -std=c++17 -O2 -mwindows -municode -DUNICODE -D_UNICODE -static ^
        src\main.cpp build\KuroCat-version.o -o KuroCat.exe ^
        -luser32 -lgdi32 -lgdiplus -lshell32 -ladvapi32 -lwinhttp -lwtsapi32 -lole32 -luuid
    if errorlevel 1 exit /b 1
    echo -- Built successfully with MinGW: %~dp0KuroCat.exe
    exit /b 0
)

echo [ERROR] Neither MSVC (cl.exe) nor MinGW (g++.exe) found in PATH.
echo Please run from "x64 Native Tools Command Prompt for VS" or install Visual Studio Build Tools.
exit /b 1
