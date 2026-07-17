@echo off
setlocal EnableExtensions
call "%~dp0robot_framework\scripts\_windows_common.bat" robot
if errorlevel 1 exit /b %errorlevel%

set "RUN_GATE=%ERESISTOR_GATE%"
if not "%~1"=="" set "RUN_GATE=%~1"

echo Running Robot Framework acceptance-criteria boundary checks, gate %RUN_GATE% ...
cd /d "%REGRESSION_ROOT%"
"%ROBOT_PYTHON%" "%ROBOT_ROOT%\run_robot.py" ^
  --profile read_only ^
  --gate "%RUN_GATE%" ^
  --output "%ERESISTOR_OUTPUT_DIR%" ^
  --host "%ERESISTOR_HOST%" ^
  --http-port "%ERESISTOR_HTTP_PORT%" ^
  --scpi-port "%ERESISTOR_SCPI_PORT%" ^
  --timeout "%ERESISTOR_TIMEOUT%" ^
  --heap-drift-limit "%ERESISTOR_HEAP_DRIFT_LIMIT%" ^
  --latency-regression-percent "%ERESISTOR_LATENCY_REGRESSION_PERCENT%" ^
  --hil-error-limit-percent "%ERESISTOR_HIL_ERROR_LIMIT_PERCENT%" ^
  --hil-stability-percent "%ERESISTOR_HIL_STABILITY_PERCENT%" ^
  --hil-off-min-ohm "%ERESISTOR_HIL_OFF_MIN_OHM%" ^
  --include criteria
exit /b %errorlevel%
