@echo off
setlocal
cd /d "%~dp0"
echo WARNING: Building Gate 3 fault-injection TEST firmware.
echo This image is for controlled regression only and is not a production release.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\tools\arduino_cli\arduino_cli.ps1" -Action Build -TestBuild %*
exit /b %ERRORLEVEL%
