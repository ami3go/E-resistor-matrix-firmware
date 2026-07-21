@echo off
setlocal EnableExtensions
set "REGRESSION_ROOT=%~dp0.."
for %%I in ("%REGRESSION_ROOT%") do set "REGRESSION_ROOT=%%~fI"
cd /d "%REGRESSION_ROOT%"

echo ============================================================
echo E-Resistor Robot Framework environment setup
echo ============================================================

set "PYTHON_CMD="
where py >nul 2>&1 && set "PYTHON_CMD=py -3"
if not defined PYTHON_CMD where python >nul 2>&1 && set "PYTHON_CMD=python"
if not defined PYTHON_CMD (
    echo ERROR: Python 3.10 or newer was not found in PATH.
    exit /b 2
)

if not exist ".venv\Scripts\python.exe" (
    echo Creating .venv ...
    %PYTHON_CMD% -m venv .venv
    if errorlevel 1 exit /b %errorlevel%
) else (
    echo Existing .venv found.
)
set "VENV_PYTHON=%CD%\.venv\Scripts\python.exe"

"%VENV_PYTHON%" -m pip --version >nul 2>&1
if errorlevel 1 (
    echo pip is missing from .venv. Repairing with ensurepip ...
    "%VENV_PYTHON%" -m ensurepip --upgrade
    if errorlevel 1 exit /b %errorlevel%
)

"%VENV_PYTHON%" -m pip install --upgrade pip setuptools wheel
if errorlevel 1 exit /b %errorlevel%
"%VENV_PYTHON%" -m pip install -e .
if errorlevel 1 exit /b %errorlevel%

if not exist "robot_frameworkariablesench_config.local.bat" (
    copy /Y "robot_frameworkariablesench_config.example.bat" "robot_frameworkariablesench_config.local.bat" >nul
    echo Created robot_frameworkariablesench_config.local.bat
    echo Edit that file before running hardware tests.
)

echo.
echo Running offline harness tests ...
"%VENV_PYTHON%" -m unittest discover -s tests -v
if errorlevel 1 exit /b %errorlevel%

echo.
echo Validating Robot suites and keyword mappings ...
"%VENV_PYTHON%" "robot_framework\cialidate_robot_suites.py"
if errorlevel 1 exit /b %errorlevel%

echo.
echo Setup completed successfully.
echo Python: "%VENV_PYTHON%"
echo Next: edit robot_frameworkariablesench_config.local.bat
exit /b 0
