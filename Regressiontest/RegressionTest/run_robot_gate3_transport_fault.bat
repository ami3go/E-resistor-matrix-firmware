@echo off
setlocal EnableExtensions
call "%~dp0robot_framework\scripts\_windows_common.bat"
if errorlevel 1 exit /b %errorlevel%

if /I not "%ERESISTOR_ALLOW_ACTIVE_OUTPUT_TESTS%"=="true" (
    echo ERROR: Gate 3 fault-injection HIL is disabled.
    echo Set ERESISTOR_ALLOW_ACTIVE_OUTPUT_TESTS=true only after verifying the fixture.
    exit /b 3
)
if not "%ERESISTOR_FIXTURE_CONFIRMATION%"=="E_RESISTOR_SINGLE_CHANNEL_DMM" (
    echo ERROR: Fixture confirmation is missing or incorrect.
    exit /b 3
)

set "RUN_GATE=G3"
if not "%~1"=="" set "RUN_GATE=%~1"
if /I not "%RUN_GATE%"=="G3" (
    echo ERROR: This fault-injection profile is defined for Gate G3 only.
    exit /b 3
)

echo ============================================================
echo GATE 3 FAULT-INJECTION HARDWARE TEST
echo This requires firmware built with build_firmware_gate3_test.bat.
echo The test invalidates one queued command and causes one Core 1 timeout while outputs are OFF.
echo DMM channel: %ERESISTOR_HIL_CHANNEL%
echo Reboot and flash the production image after this test.
echo ============================================================

cd /d "%REGRESSION_ROOT%"
"%ROBOT_PYTHON%" "%ROBOT_ROOT%\run_robot.py" ^
  --profile gate3_transport_fault ^
  --gate G3 ^
  --output "%ERESISTOR_OUTPUT_DIR%" ^
  --host "%ERESISTOR_HOST%" ^
  --http-port "%ERESISTOR_HTTP_PORT%" ^
  --scpi-port "%ERESISTOR_SCPI_PORT%" ^
  --timeout "%ERESISTOR_TIMEOUT%" ^
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
  --hil-off-min-ohm "%ERESISTOR_HIL_OFF_MIN_OHM%" ^
  --hil-sample-count "%ERESISTOR_HIL_SAMPLE_COUNT%" ^
  --hil-sample-interval "%ERESISTOR_HIL_SAMPLE_INTERVAL%"
exit /b %errorlevel%
