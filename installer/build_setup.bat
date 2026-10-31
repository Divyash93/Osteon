@echo off
rem Builds the branded Osteon setup program: installer\setup.cpp + resources (banner, build\Osteon.exe, README) -> %2\OsteonSetup.exe
rem Usage: build_setup.bat <version> <output folder>
setlocal
set VERSION=%1
set OUT=%2
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`%VSWHERE% -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSDIR=%%i
if not defined VSDIR (echo Visual Studio 2022 with C++ tools not found & exit /b 1)
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
pushd "%~dp0"
set TMPB=%TEMP%\osteon_setup_build
if not exist "%TMPB%" mkdir "%TMPB%"
rc /nologo /fo "%TMPB%\setup.res" setup.rc || (popd & exit /b 1)
cl /nologo /EHsc /std:c++20 /O1 /MT /DUNICODE /D_UNICODE /DOSTEON_VERSION=\"%VERSION%\" setup.cpp "%TMPB%\setup.res" /Fo"%TMPB%\\" /Fe:"%OUT%\OsteonSetup.exe" /link /SUBSYSTEM:WINDOWS || (popd & exit /b 1)
popd
exit /b 0
