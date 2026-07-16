@echo off
setlocal EnableExtensions
call "%~dp0robot_framework\scripts\_windows_common.bat" python
if errorlevel 1 exit /b %errorlevel%
cd /d "%REGRESSION_ROOT%"
"%REGRESSION_PYTHON%" -m unittest discover -s tests -v
if errorlevel 1 exit /b %errorlevel%
"%REGRESSION_PYTHON%" "%REGRESSION_ROOT%\run_regression.py" --help > "%TEMP%\e_resistor_python_cli_help.txt"
if errorlevel 1 exit /b %errorlevel%
echo Pure Python regression harness validation passed.
exit /b 0
