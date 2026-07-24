@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0extract_calibration_from_report.ps1" %*
exit /b %ERRORLEVEL%
