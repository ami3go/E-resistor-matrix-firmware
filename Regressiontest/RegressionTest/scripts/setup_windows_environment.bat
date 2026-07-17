@echo off
setlocal EnableExtensions
set "REGRESSION_ROOT=%~dp0.."
for %%I in ("%REGRESSION_ROOT%") do set "REGRESSION_ROOT=%%~fI"
cd /d "%REGRESSION_ROOT%"

set "SETUP_KIND=%~1"
if "%SETUP_KIND%"=="" set "SETUP_KIND=python"

echo ============================================================
echo E-Resistor %SETUP_KIND% regression environment setup
echo ============================================================

set "PYTHON_CMD="
where py >nul 2>&1 && set "PYTHON_CMD=py -3"
if not defined PYTHON_CMD (
    where python >nul 2>&1 && set "PYTHON_CMD=python"
)
if not defined PYTHON_CMD (
    echo ERROR: Python 3.10 or newer was not found in PATH.
    echo Install Python and enable the Add Python to PATH option.
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

rem Some Windows Python installations can create a venv without pip.
"%VENV_PYTHON%" -m pip --version >nul 2>&1
if errorlevel 1 (
    echo pip is missing from .venv. Repairing it with ensurepip ...
    "%VENV_PYTHON%" -m ensurepip --upgrade
    if errorlevel 1 (
        echo ERROR: Could not install pip into .venv.
        echo Remove .venv and rerun this setup, or repair the Python installation.
        exit /b 2
    )
)

"%VENV_PYTHON%" -m pip install --upgrade pip setuptools wheel
if errorlevel 1 exit /b %errorlevel%

if /I "%SETUP_KIND%"=="robot" (
    "%VENV_PYTHON%" -m pip install -e ".[robot]"
) else (
    "%VENV_PYTHON%" -m pip install -e ".[hil]"
)
if errorlevel 1 exit /b %errorlevel%

if not exist "robot_framework\variables\bench_config.local.bat" (
    copy /Y "robot_framework\variables\bench_config.example.bat" "robot_framework\variables\bench_config.local.bat" >nul
    echo Created robot_framework\variables\bench_config.local.bat
    echo Edit that file before running HIL or source/build tests.
)

if /I "%SETUP_KIND%"=="robot" (
    echo.
    echo Validating Robot suites ...
    "%VENV_PYTHON%" "robot_framework\ci\validate_robot_suites.py"
    if errorlevel 1 exit /b %errorlevel%
) else (
    echo.
    echo Running Python harness self-tests ...
    "%VENV_PYTHON%" -m unittest discover -s tests -v
    if errorlevel 1 exit /b %errorlevel%
)

echo.
echo Setup completed successfully.
echo Python: "%VENV_PYTHON%"
echo Next: edit robot_framework\variables\bench_config.local.bat
exit /b 0
