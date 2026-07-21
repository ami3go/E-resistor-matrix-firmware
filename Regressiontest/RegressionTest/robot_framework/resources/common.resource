*** Settings ***
Library      ../libraries/e_resistor_robot_library.py    WITH NAME    EResistor
Variables    ../variables/bench.py

*** Keywords ***
Initialize Regression Profile
    [Arguments]    ${profile}
    EResistor.Initialize E-Resistor Regression Suite
    ...    output_dir=${OUTPUT_DIR}
    ...    profile=${profile}
    ...    gate=${GATE}
    ...    host=${ERESISTOR_HOST}
    ...    http_port=${HTTP_PORT}
    ...    scpi_port=${SCPI_PORT}
    ...    timeout_s=${TIMEOUT_S}
    ...    iterations=${ITERATIONS}
    ...    stress_iterations=${STRESS_ITERATIONS}
    ...    heap_drift_limit_bytes=${HEAP_DRIFT_LIMIT_BYTES}
    ...    latency_regression_percent=${LATENCY_REGRESSION_PERCENT}
    ...    baseline=${BASELINE}
    ...    gate_manifest=${GATE_MANIFEST}
    ...    allow_output_tests=${ALLOW_OUTPUT_TESTS}
    ...    allow_active_output_tests=${ALLOW_ACTIVE_OUTPUT_TESTS}
    ...    allow_storage_tests=${ALLOW_STORAGE_TESTS}
    ...    allow_ota_tests=${ALLOW_OTA_TESTS}
    ...    allow_watchdog_tests=${ALLOW_WATCHDOG_TESTS}
    ...    serial_port=${SERIAL_PORT}
    ...    serial_baud=${SERIAL_BAUD}
    ...    serial_match=${SERIAL_MATCH}
    ...    dmm_resource=${DMM_RESOURCE}
    ...    dmm_idn_contains=${DMM_IDN_CONTAINS}
    ...    dmm_backend=${DMM_BACKEND}
    ...    dmm_init_commands=${DMM_INIT_COMMANDS}
    ...    dmm_measure_command=${DMM_MEASURE_COMMAND}
    ...    hil_channel=${HIL_CHANNEL}
    ...    hil_bits=${HIL_BITS}
    ...    hil_combination_masks=${HIL_COMBINATION_MASKS}
    ...    hil_repeat_cycles=${HIL_REPEAT_CYCLES}
    ...    hil_error_limit_percent=${HIL_ERROR_LIMIT_PERCENT}
    ...    hil_settle_timeout_s=${HIL_SETTLE_TIMEOUT_S}
    ...    hil_sample_count=${HIL_SAMPLE_COUNT}
    ...    hil_sample_interval_s=${HIL_SAMPLE_INTERVAL_S}
    ...    hil_stability_percent=${HIL_STABILITY_PERCENT}
    ...    hil_minimum_wait_s=${HIL_MINIMUM_WAIT_S}
    ...    hil_off_min_ohm=${HIL_OFF_MIN_OHM}
    ...    hil_serial_fault_patterns=${HIL_SERIAL_FAULT_PATTERNS}
    ...    fixture_confirmation=${FIXTURE_CONFIRMATION}
    ...    verbose=${VERBOSE}

Finalize Regression Profile
    EResistor.Finalize E-Resistor Regression Suite

Run Regression Check
    [Arguments]    ${test_id}    ${name}    ${method}
    EResistor.Run E-Resistor Regression Check    ${test_id}    ${name}    ${method}

Safety Cleanup
    EResistor.Safety Cleanup After Test
