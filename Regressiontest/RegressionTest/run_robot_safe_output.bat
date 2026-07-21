@echo off
setlocal EnableExtensions
call "%~dp0robot_framework\scripts\_windows_common.bat"
if errorlevel 1 exit /b %errorlevel%

set "RUN_GATE=G4"
if not "%~1"=="" set "RUN_GATE=%~1"
if /I not "%ERESISTOR_GATE%"=="%RUN_GATE%" echo NOTE: package default gate is %RUN_GATE%; bench_config.local.bat contains %ERESISTOR_GATE%.

echo Running E-Resistor safe-output regression, gate %RUN_GATE% ...
echo This profile only applies zero masks and repeated ALL:OFF commands.
cd /d "%REGRESSION_ROOT%"
"%ROBOT_PYTHON%" "%ROBOT_ROOT%\run_robot.py" ^
  --profile safe_output ^
  --gate "%RUN_GATE%" ^
  --output "%ERESISTOR_OUTPUT_DIR%" ^
  --host "%ERESISTOR_HOST%" ^
  --http-port "%ERESISTOR_HTTP_PORT%" ^
  --scpi-port "%ERESISTOR_SCPI_PORT%" ^
  --timeout "%ERESISTOR_TIMEOUT%" ^
  --iterations "%ERESISTOR_ITERATIONS%" ^
  --stress-iterations "%ERESISTOR_STRESS_ITERATIONS%" ^
  --heap-drift-limit "%ERESISTOR_HEAP_DRIFT_LIMIT%" ^
  --latency-regression-percent "%ERESISTOR_LATENCY_REGRESSION_PERCENT%" ^
  --baseline "%ERESISTOR_BASELINE%" ^
  --gate-manifest "%ERESISTOR_GATE_MANIFEST%" ^
  --allow-output-tests
exit /b %errorlevel%
