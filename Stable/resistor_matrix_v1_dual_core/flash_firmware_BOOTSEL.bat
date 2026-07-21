@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0flash_firmware_BOOTSEL.ps1" %*
exit /b %ERRORLEVEL%
