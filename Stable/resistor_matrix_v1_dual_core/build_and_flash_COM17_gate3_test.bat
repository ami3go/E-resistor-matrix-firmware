@echo off
setlocal
cd /d "%~dp0"
echo WARNING: Building and flashing Gate 3 fault-injection TEST firmware to COM17.
echo Confirm all E-Resistor outputs and loads are in a safe bench state.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\tools\arduino_cli\arduino_cli.ps1" -Action BuildFlash -Port COM17 -TestBuild %*
exit /b %ERRORLEVEL%
