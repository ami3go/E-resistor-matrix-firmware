@echo off
setlocal EnableExtensions
call "%~dp0robot_framework\scripts\_windows_common.bat" robot
if errorlevel 1 exit /b %errorlevel%
cd /d "%REGRESSION_ROOT%"
"%ROBOT_PYTHON%" "%ROBOT_ROOT%\scripts\generate_libdoc.py"
exit /b %errorlevel%
