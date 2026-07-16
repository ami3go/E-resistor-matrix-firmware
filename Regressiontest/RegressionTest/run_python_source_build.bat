@echo off
setlocal EnableExtensions
call "%~dp0robot_framework\scripts\_windows_common.bat" python
if errorlevel 1 exit /b %errorlevel%

set "RUN_GATE=%ERESISTOR_GATE%"
if not "%~1"=="" set "RUN_GATE=%~1"
for /f %%I in ('"%REGRESSION_PYTHON%" "%REGRESSION_ROOT%\scripts\make_timestamp.py"') do set "RUN_STAMP=%%I"
if "%ERESISTOR_SOURCE_DIR%"=="" (
    echo ERROR: ERESISTOR_SOURCE_DIR is empty.
    echo Set it in robot_framework\variables\bench_config.local.bat.
    exit /b 3
)
if not exist "%ERESISTOR_SOURCE_DIR%" (
    echo ERROR: Firmware source directory does not exist:
    echo   %ERESISTOR_SOURCE_DIR%
    exit /b 3
)
set "OPTIONAL_ARGS="
if not "%ERESISTOR_BASELINE%"=="" set OPTIONAL_ARGS=%OPTIONAL_ARGS% --baseline "%ERESISTOR_BASELINE%"
if not "%ERESISTOR_GATE_MANIFEST%"=="" set OPTIONAL_ARGS=%OPTIONAL_ARGS% --gate-manifest "%ERESISTOR_GATE_MANIFEST%"
if not "%ERESISTOR_ARDUINO_CLI%"=="" set OPTIONAL_ARGS=%OPTIONAL_ARGS% --arduino-cli "%ERESISTOR_ARDUINO_CLI%"
if not "%ERESISTOR_FQBN%"=="" set OPTIONAL_ARGS=%OPTIONAL_ARGS% --fqbn "%ERESISTOR_FQBN%"
set "RUN_OUTPUT=%ERESISTOR_PYTHON_OUTPUT_DIR%\%RUN_GATE%\source_build_%RUN_STAMP%"
echo Running pure Python source/build regression, gate %RUN_GATE% ...
cd /d "%REGRESSION_ROOT%"
"%REGRESSION_PYTHON%" "%REGRESSION_ROOT%\run_regression.py" ^
  --profile read_only ^
  --gate "%RUN_GATE%" ^
  --output "%RUN_OUTPUT%" ^
  --source-dir "%ERESISTOR_SOURCE_DIR%" ^
  --skip-device ^
  %OPTIONAL_ARGS%
set "RC=%errorlevel%"
echo Results: %RUN_OUTPUT%
exit /b %RC%
