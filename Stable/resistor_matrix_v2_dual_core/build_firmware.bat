@echo off
setlocal
cd /d "%~dp0"
echo E-Resistor firmware build - standard live mode
echo A heartbeat is printed every 3 seconds while Arduino CLI is quiet.
echo Use build_firmware_verbose.bat for complete compiler command output.
echo.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\arduino_cli\arduino_cli.ps1" -Action Build %*
set "RC=%ERRORLEVEL%"
if not "%RC%"=="0" echo Firmware build FAILED with exit code %RC%.
exit /b %RC%
