@echo off
setlocal EnableExtensions

set "REGRESSION_ROOT=%~dp0.."
for %%I in ("%REGRESSION_ROOT%") do set "REGRESSION_ROOT=%%~fI"
pushd "%REGRESSION_ROOT%" >nul 2>&1
if errorlevel 1 (
    echo ERROR: Cannot enter RegressionTest directory: "%REGRESSION_ROOT%"
    exit /b 2
)

echo ============================================================
echo E-Resistor Robot Framework environment setup
echo ============================================================

set "PYTHON_CMD="
where py >nul 2>&1
if not errorlevel 1 set "PYTHON_CMD=py -3"
if not defined PYTHON_CMD (
    where python >nul 2>&1
    if not errorlevel 1 set "PYTHON_CMD=python"
)
if not defined PYTHON_CMD (
    echo ERROR: Python 3.10 or newer was not found in PATH.
    goto :fail_not_found
)

%PYTHON_CMD% -c "import sys; raise SystemExit(0 if sys.version_info >= (3, 10) else 1)" >nul 2>&1
if errorlevel 1 (
    echo ERROR: Python 3.10 or newer is required.
    %PYTHON_CMD% --version
    goto :fail
)

if not exist ".venv\Scripts\python.exe" (
    echo Creating .venv ...
    %PYTHON_CMD% -m venv ".venv"
    if errorlevel 1 goto :fail
) else (
    echo Existing .venv found.
)

set "VENV_PYTHON=%CD%\.venv\Scripts\python.exe"
if not exist "%VENV_PYTHON%" (
    echo ERROR: Virtual-environment Python was not created: "%VENV_PYTHON%"
    goto :fail
)

"%VENV_PYTHON%" -m pip --version >nul 2>&1
if errorlevel 1 (
    echo pip is missing from .venv. Repairing with ensurepip ...
    "%VENV_PYTHON%" -m ensurepip --upgrade
    if errorlevel 1 goto :fail
)

"%VENV_PYTHON%" -m pip install --upgrade pip setuptools wheel
if errorlevel 1 goto :fail

"%VENV_PYTHON%" -m pip install -e .
if errorlevel 1 goto :fail

set "BENCH_CONFIG_EXAMPLE=robot_framework\variables\bench_config.example.bat"
set "BENCH_CONFIG_LOCAL=robot_framework\variables\bench_config.local.bat"
if not exist "%BENCH_CONFIG_LOCAL%" (
    if not exist "%BENCH_CONFIG_EXAMPLE%" (
        echo ERROR: Missing bench configuration template: "%BENCH_CONFIG_EXAMPLE%"
        goto :fail
    )
    copy /Y "%BENCH_CONFIG_EXAMPLE%" "%BENCH_CONFIG_LOCAL%" >nul
    if errorlevel 1 (
        echo ERROR: Failed to create "%BENCH_CONFIG_LOCAL%".
        goto :fail
    )
    echo Created %BENCH_CONFIG_LOCAL%
    echo Edit that file before running hardware tests.
) else (
    echo Existing %BENCH_CONFIG_LOCAL% preserved.
)

echo.
echo Running offline harness tests ...
"%VENV_PYTHON%" -m unittest discover -s tests -v
if errorlevel 1 goto :fail

echo.
echo Validating Robot suites and keyword mappings ...
"%VENV_PYTHON%" "robot_framework\ci\validate_robot_suites.py"
if errorlevel 1 goto :fail

echo.
echo Setup completed successfully.
"%VENV_PYTHON%" --version
"%VENV_PYTHON%" -c "import e_resistor_regression, robot; print('Regression package:', e_resistor_regression.__version__); print('Robot Framework:', robot.__version__)"
echo Bench configuration: %BENCH_CONFIG_LOCAL%
popd
exit /b 0

:fail_not_found
set "SETUP_RC=2"
goto :fail_exit

:fail
set "SETUP_RC=%errorlevel%"
if "%SETUP_RC%"=="0" set "SETUP_RC=1"

:fail_exit
echo.
echo ERROR: E-Resistor Robot Framework environment setup failed. Exit code %SETUP_RC%.
popd
exit /b %SETUP_RC%
