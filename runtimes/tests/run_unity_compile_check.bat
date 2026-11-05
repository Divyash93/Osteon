@echo off
rem Type-checks the Unity package against small stand-ins for the UnityEngine / UnityEditor APIs it uses.
rem This does NOT run it in Unity: it only catches syntax and type errors on a machine without Unity installed.
setlocal
set CSC=%ProgramFiles%\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\Roslyn\csc.exe
if not exist "%CSC%" set CSC=%ProgramFiles%\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\Roslyn\csc.exe
set ROOT=%~dp0..\..
set PKG=%ROOT%\runtimes\unity\com.osteon2d.runtime
if not exist "%ROOT%\build\rt" mkdir "%ROOT%\build\rt"
"%CSC%" -nologo -target:library -out:"%ROOT%\build\rt\UnityCheck.dll" "%~dp0UnityStubs.cs" "%PKG%\Runtime\Core\*.cs" "%PKG%\Runtime\Unity\*.cs" "%PKG%\Editor\*.cs" && echo Unity package type-checks.
