@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\arduino_cli\arduino_cli.ps1" -Action Setup %*
set "RC=%ERRORLEVEL%"
if not "%RC%"=="0" echo Arduino CLI setup FAILED with exit code %RC%.
exit /b %RC%
