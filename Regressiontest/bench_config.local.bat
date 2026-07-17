@echo off
rem Copy this file to bench_config.local.bat and edit the values for your bench.
rem bench_config.local.bat is intentionally not included in future release archives,
rem so it can remain unchanged when RegressionTest is upgraded in place.

set "ERESISTOR_HOST=192.168.0.55"
set "ERESISTOR_HTTP_PORT=80"
set "ERESISTOR_SCPI_PORT=5025"
set "ERESISTOR_TIMEOUT=3.0"
set "ERESISTOR_GATE=G0"
set "ERESISTOR_OUTPUT_DIR=%REGRESSION_ROOT%\results\robot"
set "ERESISTOR_PYTHON_OUTPUT_DIR=%REGRESSION_ROOT%\results\python"

rem RP2040 USB CDC diagnostic port.
set "ERESISTOR_SERIAL_PORT=COM17"
set "ERESISTOR_SERIAL_BAUD=115200"
set "ERESISTOR_SERIAL_MATCH="

rem USB/VISA DMM. Use auto or a complete VISA resource string.
set "ERESISTOR_DMM_RESOURCE=USB0::0x03EB::0x2065::HEWLETT-PACKARD_34401A_0_11-5-2::INSTR"
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
rem set "ERESISTOR_ALLOW_ACTIVE_OUTPUT_TESTS=true"
rem "ERESISTOR_FIXTURE_CONFIRMATION="
rem To authorize the fixture, use exactly:
set "ERESISTOR_ALLOW_ACTIVE_OUTPUT_TESTS=true"
set "ERESISTOR_FIXTURE_CONFIRMATION=E_RESISTOR_SINGLE_CHANNEL_DMM"

rem Source/build test settings.
rem Example: set "ERESISTOR_SOURCE_DIR=C:\path\to\resistor_matrix_v1_dual_core"
set "ERESISTOR_SOURCE_DIR=C:\Users\achestni\Documents\GitHub\E-Resistors\E-resistor-matrix-firmware\Stable\resistor_matrix_v1_dual_core"
set "ERESISTOR_ARDUINO_CLI=arduino-cli"
set "ERESISTOR_FQBN=rp2040:rp2040:waveshare_rp2040_zero:flash=2097152_1048576"
set "ERESISTOR_BASELINE="
set "ERESISTOR_GATE_MANIFEST="
