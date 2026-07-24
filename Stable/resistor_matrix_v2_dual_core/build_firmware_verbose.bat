@echo off
setlocal
cd /d "%~dp0"
echo E-Resistor firmware build - VERBOSE live mode
echo Every Arduino CLI and compiler line will be shown and logged.
echo.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\arduino_cli\arduino_cli.ps1" -Action Build -VerboseBuild %*
set "RC=%ERRORLEVEL%"
if not "%RC%"=="0" echo Verbose firmware build FAILED with exit code %RC%.
exit /b %RC%
