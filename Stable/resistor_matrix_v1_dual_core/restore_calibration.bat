@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0restore_calibration.ps1" %*
exit /b %ERRORLEVEL%
