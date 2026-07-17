@echo off
setlocal
cd /d "%~dp0"
echo E-Resistor verbose build and flash on COM17
echo Every compiler command will be shown and logged.
echo.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\arduino_cli\arduino_cli.ps1" -Action BuildFlash -Port COM17 -VerboseBuild %*
set "RC=%ERRORLEVEL%"
if not "%RC%"=="0" echo Verbose build and upload FAILED with exit code %RC%.
exit /b %RC%
