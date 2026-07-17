@echo off
setlocal EnableExtensions
call "%~dp0robot_framework\scripts\_windows_common.bat"
if errorlevel 1 exit /b %errorlevel%

if "%~1"=="" (
    echo Usage:
    echo   run_robot_custom.bat --profile read_only --gate G2 --host 192.168.0.55
    echo.
    "%ROBOT_PYTHON%" "%ROBOT_ROOT%\run_robot.py" --help
    exit /b 1
)

cd /d "%REGRESSION_ROOT%"
"%ROBOT_PYTHON%" "%ROBOT_ROOT%\run_robot.py" %*
exit /b %errorlevel%
