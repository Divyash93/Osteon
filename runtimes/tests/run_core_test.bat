@echo off
rem Builds the C# runtime core with the Roslyn compiler that ships with Visual Studio and checks it against the C++ engine's golden poses.
setlocal
set CSC=%ProgramFiles%\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\Roslyn\csc.exe
if not exist "%CSC%" set CSC=%ProgramFiles%\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\Roslyn\csc.exe
if not exist "%CSC%" (echo csc.exe not found & exit /b 1)
set ROOT=%~dp0..\..
if not exist "%ROOT%\build\rt" mkdir "%ROOT%\build\rt"
"%CSC%" -nologo -out:"%ROOT%\build\rt\CoreTest.exe" "%~dp0CoreTest.cs" "%ROOT%\runtimes\unity\com.osteon2d.runtime\Runtime\Core\*.cs" || exit /b 1
"%ROOT%\build\rt\CoreTest.exe" "%ROOT%\samples\character_rig.osteon" "%~dp0golden\character_rig.json"
