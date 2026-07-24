@echo off
setlocal EnableExtensions
call "%~dp0robot_framework\scripts\_windows_common.bat"
if errorlevel 1 exit /b %errorlevel%

if /I not "%ERESISTOR_ALLOW_ACTIVE_OUTPUT_TESTS%"=="true" (
    echo ERROR: Active HIL output tests are disabled.
    echo Edit robot_framework\variables\bench_config.local.bat and set:
    echo   set "ERESISTOR_ALLOW_ACTIVE_OUTPUT_TESTS=true"
    exit /b 3
)
if not "%ERESISTOR_FIXTURE_CONFIRMATION%"=="E_RESISTOR_SINGLE_CHANNEL_DMM" (
    echo ERROR: Fixture confirmation is missing or incorrect.
    echo Verify the DMM is connected only to the selected channel, then set:
    echo   set "ERESISTOR_FIXTURE_CONFIRMATION=E_RESISTOR_SINGLE_CHANNEL_DMM"
    exit /b 3
)

set "RUN_GATE=G5"
if not "%~1"=="" set "RUN_GATE=%~1"
if /I not "%ERESISTOR_GATE%"=="%RUN_GATE%" echo NOTE: package default gate is %RUN_GATE%; bench_config.local.bat contains %ERESISTOR_GATE%.

echo ============================================================
echo ACTIVE HARDWARE TEST
echo Channel: %ERESISTOR_HIL_CHANNEL%
echo COM port: %ERESISTOR_SERIAL_PORT%
echo DMM:      %ERESISTOR_DMM_RESOURCE%
echo Gate:     %RUN_GATE%
echo ============================================================

cd /d "%REGRESSION_ROOT%"
"%ROBOT_PYTHON%" "%ROBOT_ROOT%\run_robot.py" ^
  --profile hil_single_channel ^
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
  --allow-active-output-tests ^
  --fixture-confirmation "%ERESISTOR_FIXTURE_CONFIRMATION%" ^
  --serial-port "%ERESISTOR_SERIAL_PORT%" ^
  --serial-baud "%ERESISTOR_SERIAL_BAUD%" ^
  --serial-match "%ERESISTOR_SERIAL_MATCH%" ^
  --dmm-resource "%ERESISTOR_DMM_RESOURCE%" ^
  --dmm-idn-contains "%ERESISTOR_DMM_IDN_CONTAINS%" ^
  --dmm-backend "%ERESISTOR_DMM_BACKEND%" ^
  --dmm-init-commands "%ERESISTOR_DMM_INIT_COMMANDS%" ^
  --dmm-measure-command "%ERESISTOR_DMM_MEASURE_COMMAND%" ^
  --hil-channel "%ERESISTOR_HIL_CHANNEL%" ^
  --hil-bits "%ERESISTOR_HIL_BITS%" ^
  --hil-combination-masks "%ERESISTOR_HIL_COMBINATION_MASKS%" ^
  --hil-repeat-cycles "%ERESISTOR_HIL_REPEAT_CYCLES%" ^
  --hil-error-limit-percent "%ERESISTOR_HIL_ERROR_LIMIT_PERCENT%" ^
  --hil-settle-timeout "%ERESISTOR_HIL_SETTLE_TIMEOUT%" ^
  --hil-sample-count "%ERESISTOR_HIL_SAMPLE_COUNT%" ^
  --hil-sample-interval "%ERESISTOR_HIL_SAMPLE_INTERVAL%" ^
  --hil-stability-percent "%ERESISTOR_HIL_STABILITY_PERCENT%" ^
  --hil-minimum-wait "%ERESISTOR_HIL_MINIMUM_WAIT%" ^
  --hil-off-min-ohm "%ERESISTOR_HIL_OFF_MIN_OHM%" ^
  --hil-serial-fault-patterns "%ERESISTOR_HIL_SERIAL_FAULT_PATTERNS%"
exit /b %errorlevel%
