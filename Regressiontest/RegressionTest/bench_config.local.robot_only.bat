@echo off
rem E-Resistor Robot Framework bench configuration.
rem Store this file as:
rem   RegressionTest\robot_framework\variables\bench_config.local.bat
rem
rem This local file is preserved when the regression package is upgraded.

rem ---------------------------------------------------------------------------
rem E-Resistor network connection
rem ---------------------------------------------------------------------------
set "ERESISTOR_HOST=192.168.0.55"
set "ERESISTOR_HTTP_PORT=80"
set "ERESISTOR_SCPI_PORT=5025"
set "ERESISTOR_TIMEOUT=3.0"

rem Current optimization gate. A gate supplied on the launcher command line
rem takes precedence, for example: run_robot_hil_single_channel.bat G2
set "ERESISTOR_GATE=G2"

rem Robot Framework evidence output
set "ERESISTOR_OUTPUT_DIR=%REGRESSION_ROOT%\results\robot"

rem ---------------------------------------------------------------------------
rem General regression limits
rem ---------------------------------------------------------------------------
set "ERESISTOR_ITERATIONS=30"
set "ERESISTOR_STRESS_ITERATIONS=100"
set "ERESISTOR_HEAP_DRIFT_LIMIT=2048"
set "ERESISTOR_LATENCY_REGRESSION_PERCENT=15.0"

rem Optional accepted baseline for gate-to-gate comparison.
rem Leave empty to disable baseline comparison.
set "ERESISTOR_BASELINE="

rem ---------------------------------------------------------------------------
rem RP2040 USB CDC diagnostic connection
rem ---------------------------------------------------------------------------
set "ERESISTOR_SERIAL_PORT=COM17"
set "ERESISTOR_SERIAL_BAUD=115200"
set "ERESISTOR_SERIAL_MATCH="

rem ---------------------------------------------------------------------------
rem USB/VISA DMM
rem ---------------------------------------------------------------------------
set "ERESISTOR_DMM_RESOURCE=USB0::0x03EB::0x2065::HEWLETT-PACKARD_34401A_0_11-5-2::INSTR"
set "ERESISTOR_DMM_IDN_CONTAINS=34401"
set "ERESISTOR_DMM_BACKEND="
set "ERESISTOR_DMM_INIT_COMMANDS=*CLS|CONF:RES AUTO|TRIG:SOUR IMM|SAMP:COUN 1"
set "ERESISTOR_DMM_MEASURE_COMMAND=READ?"

rem ---------------------------------------------------------------------------
rem Single-channel hardware-in-the-loop configuration
rem ---------------------------------------------------------------------------
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

rem ---------------------------------------------------------------------------
rem Active-output safety interlocks
rem
rem Keep these enabled only while:
rem   1. The DMM is connected across channel 1.
rem   2. No external circuit is connected to the selected channel.
rem   3. The other seven channels are required to remain OFF.
rem ---------------------------------------------------------------------------
set "ERESISTOR_ALLOW_ACTIVE_OUTPUT_TESTS=true"
set "ERESISTOR_FIXTURE_CONFIRMATION=E_RESISTOR_SINGLE_CHANNEL_DMM"
