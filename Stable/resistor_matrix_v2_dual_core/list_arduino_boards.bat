@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\arduino_cli\arduino_cli.ps1" -Action ListBoards %*
exit /b %ERRORLEVEL%
