@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "REGRESSION_ROOT=%~dp0.."
for %%I in ("%REGRESSION_ROOT%") do set "REGRESSION_ROOT=%%~fI"
cd /d "%REGRESSION_ROOT%"

set "SETUP_KIND=%~1"
if "%SETUP_KIND%"=="" set "SETUP_KIND=python"
if /I not "%SETUP_KIND%"=="python" if /I not "%SETUP_KIND%"=="robot" (
    echo ERROR: Unknown setup kind "%SETUP_KIND%". Use python or robot.
    exit /b 2
)

set "SETUP_MODE=%~2"
if "%SETUP_MODE%"=="" set "SETUP_MODE=full"
if /I not "%SETUP_MODE%"=="full" if /I not "%SETUP_MODE%"=="ensure" (
    echo ERROR: Unknown setup mode "%SETUP_MODE%". Use full or ensure.
    exit /b 2
)

set "INSTALL_TARGET=.[hil]"
if /I "%SETUP_KIND%"=="robot" set "INSTALL_TARGET=.[robot]"

if /I "%SETUP_MODE%"=="full" (
    echo ============================================================
    echo E-Resistor %SETUP_KIND% regression environment setup
    echo ============================================================
) else (
    echo Checking E-Resistor %SETUP_KIND% regression environment ...
)

set "PYTHON_CMD="
where py >nul 2>&1
if not errorlevel 1 (
    py -3 -c "import sys; raise SystemExit(0 if sys.version_info >= (3, 10) else 1)" >nul 2>&1
    if not errorlevel 1 set "PYTHON_CMD=py -3"
)
if not defined PYTHON_CMD (
    where python >nul 2>&1
    if not errorlevel 1 (
        python -c "import sys; raise SystemExit(0 if sys.version_info >= (3, 10) else 1)" >nul 2>&1
        if not errorlevel 1 set "PYTHON_CMD=python"
    )
)
if not defined PYTHON_CMD goto :python_missing

set "VENV_PYTHON=%REGRESSION_ROOT%\.venv\Scripts\python.exe"
set "VENV_RECREATED=0"

if not exist "%VENV_PYTHON%" goto :recreate_venv
"%VENV_PYTHON%" -c "import sys; raise SystemExit(0 if sys.version_info >= (3, 10) else 1)" >nul 2>&1
if errorlevel 1 (
    echo Existing .venv is invalid, moved, or uses an unsupported Python.
    goto :recreate_venv
)
goto :venv_ready

:recreate_venv
if exist ".venv" (
    echo Removing unusable .venv ...
    rmdir /s /q ".venv"
    if exist ".venv" goto :venv_remove_failed
)
echo Creating .venv ...
%PYTHON_CMD% -m venv ".venv"
if errorlevel 1 goto :venv_create_failed
set "VENV_RECREATED=1"

:venv_ready
"%VENV_PYTHON%" -m pip --version >nul 2>&1
if not errorlevel 1 goto :pip_ready

echo pip is missing from .venv. Repairing it with ensurepip ...
"%VENV_PYTHON%" -m ensurepip --upgrade --default-pip
if errorlevel 1 (
    if "%VENV_RECREATED%"=="1" goto :pip_repair_failed
    echo pip repair failed. Recreating .venv once ...
    rmdir /s /q ".venv"
    if exist ".venv" goto :venv_remove_failed
    %PYTHON_CMD% -m venv ".venv"
    if errorlevel 1 goto :venv_create_failed
    set "VENV_RECREATED=1"
    "%VENV_PYTHON%" -m ensurepip --upgrade --default-pip
    if errorlevel 1 goto :pip_repair_failed
)
"%VENV_PYTHON%" -m pip --version >nul 2>&1
if errorlevel 1 goto :pip_repair_failed

:pip_ready
if /I "%SETUP_MODE%"=="full" (
    echo Updating Python packaging tools ...
    "%VENV_PYTHON%" -m pip install --upgrade pip setuptools wheel
    if errorlevel 1 echo WARNING: Packaging-tool upgrade failed. Continuing with the installed versions.
)

set "ENVIRONMENT_OK=0"
if /I "%SETUP_KIND%"=="robot" (
    "%VENV_PYTHON%" -c "import importlib.metadata as m, e_resistor_regression as p, robot, serial, pyvisa; raise SystemExit(0 if m.version('e-resistor-regression') == p.__version__ else 1)" >nul 2>&1
    if not errorlevel 1 set "ENVIRONMENT_OK=1"
) else (
    "%VENV_PYTHON%" -c "import importlib.metadata as m, e_resistor_regression as p, serial, pyvisa; raise SystemExit(0 if m.version('e-resistor-regression') == p.__version__ else 1)" >nul 2>&1
    if not errorlevel 1 set "ENVIRONMENT_OK=1"
)

if /I "%SETUP_MODE%"=="full" set "ENVIRONMENT_OK=0"
if "%ENVIRONMENT_OK%"=="0" (
    echo Installing E-Resistor package and %SETUP_KIND% dependencies ...
    "%VENV_PYTHON%" -m pip install -e "%INSTALL_TARGET%"
    if errorlevel 1 goto :package_install_failed
) else (
    echo Existing .venv and required packages are ready.
)

if not exist "robot_framework\variables\bench_config.local.bat" (
    copy /Y "robot_framework\variables\bench_config.example.bat" "robot_framework\variables\bench_config.local.bat" >nul
    if errorlevel 1 goto :config_copy_failed
    echo Created robot_framework\variables\bench_config.local.bat
    echo Edit that file before running HIL or source/build tests.
)

if not exist "results\setup" mkdir "results\setup"
(
    echo E-Resistor regression environment diagnostics
    echo Setup kind: %SETUP_KIND%
    echo Setup mode: %SETUP_MODE%
    echo Root: %REGRESSION_ROOT%
    echo Base launcher: %PYTHON_CMD%
    echo Venv Python: %VENV_PYTHON%
    "%VENV_PYTHON%" -c "import datetime, platform, sys; print('Timestamp UTC:', datetime.datetime.now(datetime.timezone.utc).isoformat()); print('Python:', sys.version.replace(chr(10), ' ')); print('Executable:', sys.executable); print('Platform:', platform.platform())"
    "%VENV_PYTHON%" -m pip --version
    "%VENV_PYTHON%" -m pip show e-resistor-regression pyserial PyVISA robotframework
) > "results\setup\last_setup_diagnostics.txt" 2>&1

if /I "%SETUP_MODE%"=="ensure" (
    endlocal & exit /b 0
)

if /I "%SETUP_KIND%"=="robot" (
    echo.
    echo Validating Robot suites ...
    "%VENV_PYTHON%" "robot_framework\ci\validate_robot_suites.py"
    if errorlevel 1 goto :validation_failed
) else (
    echo.
    echo Running Python harness self-tests ...
    "%VENV_PYTHON%" -m unittest discover -s tests -v
    if errorlevel 1 goto :validation_failed
)

echo.
echo Setup completed successfully.
echo Python: "%VENV_PYTHON%"
echo Diagnostics: results\setup\last_setup_diagnostics.txt
echo Next: edit robot_framework\variables\bench_config.local.bat
endlocal & exit /b 0

:python_missing
echo ERROR: Python 3.10 or newer was not found in PATH.
echo Install Python and enable the Add Python to PATH option.
endlocal & exit /b 2

:venv_remove_failed
echo ERROR: Could not remove the unusable .venv directory.
echo Close terminals or programs using .venv and run setup again.
endlocal & exit /b 3

:venv_create_failed
echo ERROR: Could not create .venv with %PYTHON_CMD%.
endlocal & exit /b 3

:pip_repair_failed
echo ERROR: Could not install pip into .venv with ensurepip.
echo Repair or reinstall the base Python installation, then rerun setup.
endlocal & exit /b 4

:package_install_failed
echo ERROR: Could not install the E-Resistor package or required dependencies.
echo Review the pip error above and results\setup\last_setup_diagnostics.txt when available.
endlocal & exit /b 5

:config_copy_failed
echo ERROR: Could not create bench_config.local.bat.
endlocal & exit /b 6

:validation_failed
echo ERROR: Regression harness validation failed.
endlocal & exit /b 7
