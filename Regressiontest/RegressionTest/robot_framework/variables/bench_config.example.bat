@echo off
rem Copy this file to bench_config.local.bat and edit the values for your bench.
rem bench_config.local.bat is intentionally not included in future release archives,
rem so it can remain unchanged when RegressionTest is upgraded in place.

set "ERESISTOR_HOST=192.168.0.55"
set "ERESISTOR_HTTP_PORT=80"
set "ERESISTOR_SCPI_PORT=5025"
set "ERESISTOR_TIMEOUT=3.0"
set "ERESISTOR_GATE=G2"
set "ERESISTOR_OUTPUT_DIR=%REGRESSION_ROOT%\results\robot"
set "ERESISTOR_PYTHON_OUTPUT_DIR=%REGRESSION_ROOT%\results\python"

rem RP2040 USB CDC diagnostic port.
set "ERESISTOR_SERIAL_PORT=COM7"
set "ERESISTOR_SERIAL_BAUD=115200"
set "ERESISTOR_SERIAL_MATCH="

rem USB/VISA DMM. Use auto or a complete VISA resource string.
set "ERESISTOR_DMM_RESOURCE=USB0::0x0957::0x0607::MY12345678::INSTR"
set "ERESISTOR_DMM_IDN_CONTAINS=34401"
set "ERESISTOR_DMM_BACKEND="
set "ERESISTOR_DMM_INIT_COMMANDS=*CLS|CONF:RES AUTO|TRIG:SOUR IMM|SAMP:COUN 1"
set "ERESISTOR_DMM_MEASURE_COMMAND=READ?"

rem Single-channel HIL settings.
set "ERESISTOR_HIL_CHANNEL=1"
set "ERESISTOR_HIL_BITS=0-15"
set "ERESISTOR_HIL_COMBINATION_MASKS=0003,0005,0009"
set "ERESISTOR_HIL_REPEAT_CYCLES=10"
set "ERESISTOR_HIL_ERROR_LIMIT_PERCENT=1.0"
set "ERESISTOR_HIL_SETTLE_TIMEOUT=20.0"
set "ERESISTOR_HIL_SAMPLE_COUNT=5"
set "ERESISTOR_HIL_SAMPLE_INTERVAL=0.25"
set "ERESISTOR_HIL_STABILITY_PERCENT=0.20"
set "ERESISTOR_HIL_MINIMUM_WAIT=0.5"
set "ERESISTOR_HIL_OFF_MIN_OHM=50000000"
set "ERESISTOR_HIL_SERIAL_FAULT_PATTERNS=fatal|panic|assert|hardfault|queue overflow"

rem HIL remains disabled until both values below are intentionally changed.
set "ERESISTOR_ALLOW_ACTIVE_OUTPUT_TESTS=false"
set "ERESISTOR_FIXTURE_CONFIRMATION="
rem To authorize the fixture, use exactly:
rem set "ERESISTOR_ALLOW_ACTIVE_OUTPUT_TESTS=true"
rem set "ERESISTOR_FIXTURE_CONFIRMATION=E_RESISTOR_SINGLE_CHANNEL_DMM"

rem Offline source-check settings. No firmware compiler is invoked.
set "ERESISTOR_SOURCE_DIR=C:\path\to\resistor_matrix_v1_dual_core"
set "ERESISTOR_BASELINE=%REGRESSION_ROOT%\baselines\G1_hil_single_channel_20260717T103129Z\results.json"
set "ERESISTOR_GATE_MANIFEST=%REGRESSION_ROOT%\gate_acceptance.json"
