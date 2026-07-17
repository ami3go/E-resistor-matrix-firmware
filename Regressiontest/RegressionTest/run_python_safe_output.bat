@echo off
setlocal EnableExtensions
call "%~dp0robot_framework\scripts\_windows_common.bat"
if errorlevel 1 exit /b %errorlevel%

set "RUN_GATE=G2"
if not "%~1"=="" set "RUN_GATE=%~1"
if /I not "%ERESISTOR_GATE%"=="%RUN_GATE%" echo NOTE: package default gate is %RUN_GATE%; bench_config.local.bat contains %ERESISTOR_GATE%.
for /f %%I in ('"%REGRESSION_PYTHON%" "%REGRESSION_ROOT%\scripts\make_timestamp.py"') do set "RUN_STAMP=%%I"
set "OPTIONAL_ARGS="
if not "%ERESISTOR_SOURCE_DIR%"=="" set OPTIONAL_ARGS=%OPTIONAL_ARGS% --source-dir "%ERESISTOR_SOURCE_DIR%"
if not "%ERESISTOR_BASELINE%"=="" set OPTIONAL_ARGS=%OPTIONAL_ARGS% --baseline "%ERESISTOR_BASELINE%"
if not "%ERESISTOR_GATE_MANIFEST%"=="" set OPTIONAL_ARGS=%OPTIONAL_ARGS% --gate-manifest "%ERESISTOR_GATE_MANIFEST%"
set "RUN_OUTPUT=%ERESISTOR_PYTHON_OUTPUT_DIR%\%RUN_GATE%\safe_output_%RUN_STAMP%"
echo Running pure Python safe-output regression, gate %RUN_GATE% ...
echo This profile only applies zero masks and repeated ALL:OFF commands.
cd /d "%REGRESSION_ROOT%"
"%REGRESSION_PYTHON%" "%REGRESSION_ROOT%\run_regression.py" ^
  --profile safe_output ^
  --gate "%RUN_GATE%" ^
  --output "%RUN_OUTPUT%" ^
  --host "%ERESISTOR_HOST%" ^
  --http-port "%ERESISTOR_HTTP_PORT%" ^
  --scpi-port "%ERESISTOR_SCPI_PORT%" ^
  --timeout "%ERESISTOR_TIMEOUT%" ^
  --iterations "%ERESISTOR_ITERATIONS%" ^
  --stress-iterations "%ERESISTOR_STRESS_ITERATIONS%" ^
  --heap-drift-limit "%ERESISTOR_HEAP_DRIFT_LIMIT%" ^
  --latency-regression-percent "%ERESISTOR_LATENCY_REGRESSION_PERCENT%" ^
  --allow-output-tests ^
  %OPTIONAL_ARGS%
set "RC=%errorlevel%"
echo Results: %RUN_OUTPUT%
exit /b %RC%
