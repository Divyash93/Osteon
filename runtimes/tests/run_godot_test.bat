@echo off
rem Runs the GDScript runtime core against the C++ engine's golden poses with a headless Godot 4.
rem   run_godot_test.bat <path to Godot_v4.x_win64_console.exe>
setlocal
if "%~1"=="" (echo usage: run_godot_test.bat path\to\Godot_console.exe & exit /b 1)
set ROOT=%~dp0..\..
"%~1" --headless --path "%ROOT%\runtimes\godot" --import >nul 2>&1
"%~1" --headless --path "%ROOT%\runtimes\godot" --script res://tests/test_core.gd -- "%ROOT%\samples\character_rig.osteon" "%ROOT%\runtimes\tests\golden\character_rig.json"
