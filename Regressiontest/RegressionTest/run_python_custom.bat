@echo off
setlocal EnableExtensions
call "%~dp0robot_framework\scripts\_windows_common.bat" python
if errorlevel 1 exit /b %errorlevel%
if "%~1"=="" (
    echo Usage:
    echo   run_python_custom.bat --profile read_only --gate G0 --host 192.168.0.55 --output results\python\custom
    echo.
    "%REGRESSION_PYTHON%" "%REGRESSION_ROOT%\run_regression.py" --help
    exit /b 1
)
cd /d "%REGRESSION_ROOT%"
"%REGRESSION_PYTHON%" "%REGRESSION_ROOT%\run_regression.py" %*
exit /b %errorlevel%
