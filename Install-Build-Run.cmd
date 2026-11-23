@echo off
setlocal EnableExtensions DisableDelayedExpansion
cd /d "%~dp0"
set "VSINST=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
set "VSROOT="
for /f "usebackq delims=" %%I in (`"%VSINST%\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%I"
if not defined VSROOT goto :novs
set "KITS=%ProgramFiles(x86)%\Windows Kits\10"
for /d %%V in ("%KITS%\Include\10.*") do if exist "%%~V\um\Windows.h" if exist "%KITS%\Lib\%%~nxV\um\x64\gdiplus.lib" if exist "%KITS%\Lib\%%~nxV\ucrt\x64\ucrt.lib" goto :build
echo Installing Windows 11 SDK. Approve the Windows prompt and wait for setup.
powershell.exe -NoProfile -Command "$ErrorActionPreference='Stop'; try { $a='modify --installPath '+[char]34+$env:VSROOT+[char]34+' --add Microsoft.VisualStudio.Component.Windows11SDK.26100 --passive --norestart'; $p=Start-Process -FilePath (Join-Path $env:VSINST 'setup.exe') -ArgumentList $a -Verb RunAs -PassThru -Wait; exit $p.ExitCode } catch { Write-Host $_; exit 1 }"
if "%errorlevel%"=="3010" goto :reboot
if errorlevel 1 goto :failed
:build
call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 goto :failed
pushd windows
call build.bat
if errorlevel 1 goto :failed
if not exist KuroCat.exe goto :failed
start "" "%CD%\KuroCat.exe"
exit /b 0
:novs
echo Visual Studio C++ x64 build tools were not found.
goto :failed
:reboot
echo Windows requested a restart. Restart Windows and run this file again.
pause
exit /b 1
:failed
echo Setup or build did not complete. See the message above.
pause
exit /b 1
