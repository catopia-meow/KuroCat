@echo off
rem Builds and runs the Windows renderer checks. Run from a "x64 Native Tools" prompt (MSVC).
setlocal
cd /d "%~dp0"
if not exist build mkdir build
cl /nologo /std:c++17 /O2 /EHsc /utf-8 /DUNICODE /D_UNICODE /W3 render_checks.cpp /Fo:build\ /Fe:build\render_checks.exe ^
   /link gdiplus.lib gdi32.lib user32.lib ole32.lib uuid.lib oleaut32.lib
if errorlevel 1 exit /b 1
build\render_checks.exe
