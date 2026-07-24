@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\arduino_cli\arduino_cli.ps1" -Action BuildFlash -Port COM17 %*
set "RC=%ERRORLEVEL%"
if not "%RC%"=="0" echo Build and upload FAILED with exit code %RC%.
exit /b %RC%
