*** Settings ***
Documentation     Read-only regression plus explicitly authorized zero-mask and repeated ALL:OFF paths.
Resource          ../resources/common.resource
Suite Setup       Initialize Regression Profile    safe_output
Suite Teardown    Finalize Regression Profile
Test Teardown     Safety Cleanup
Force Tags        e-resistor    regression    safe-output

*** Test Cases ***
NET-001 TCP Reachability
    [Tags]    NET-001    network
    Run Regression Check    NET-001    TCP reachability    test_tcp_reachability

HTTP-001 HTTP Ping
    [Tags]    HTTP-001    http
    Run Regression Check    HTTP-001    HTTP ping    test_http_ping

HTTP-002 State Schema And Readiness
    [Tags]    HTTP-002    http    state
    Run Regression Check    HTTP-002    State schema and readiness    test_state_schema

HTTP-003 Required GUI Pages
    [Tags]    HTTP-003    http    gui
    Run Regression Check    HTTP-003    Required GUI pages    test_pages

SCPI-001 Identity And Greeting
    [Tags]    SCPI-001    scpi
    Run Regression Check    SCPI-001    SCPI identity and greeting    test_scpi_identity

SCPI-002 Status And State Consistency
    [Tags]    SCPI-002    scpi    state
    Run Regression Check    SCPI-002    SCPI status and state consistency    test_scpi_state_consistency

SCPI-003 Calibration Table Integrity
    [Tags]    SCPI-003    scpi    calibration
    Run Regression Check    SCPI-003    Calibration table integrity    test_calibration

SCPI-004 Undefined Header Error Queue
    [Tags]    SCPI-004    scpi    negative
    Run Regression Check    SCPI-004    Undefined-header error queue    test_scpi_error_queue

SCPI-005 Oversized Line Recovery
    [Tags]    SCPI-005    scpi    negative
    Run Regression Check    SCPI-005    Oversized-line recovery    test_scpi_overflow_recovery

SCPI-006 Read-Only Target-Mask Calculation
    [Tags]    SCPI-006    scpi    target-search    gate2
    Run Regression Check    SCPI-006    Read-only target-mask calculation    test_scpi_target_calculation

PERF-001 HTTP Latency Sample
    [Tags]    PERF-001    performance    http
    Run Regression Check    PERF-001    HTTP latency sample    test_http_latency

PERF-002 SCPI Latency Sample
    [Tags]    PERF-002    performance    scpi
    Run Regression Check    PERF-002    SCPI latency sample    test_scpi_latency

MEM-001 Repeated State Heap Stability
    [Tags]    MEM-001    memory    stress
    Run Regression Check    MEM-001    Repeated-state heap stability    test_heap_stability

STRESS-001 Concurrent HTTP And SCPI Polling
    [Tags]    STRESS-001    stress    http    scpi
    Run Regression Check    STRESS-001    Concurrent HTTP and SCPI polling    test_concurrent_polling

FILES-001 Calibration Bundle Download
    [Tags]    FILES-001    files    calibration
    Run Regression Check    FILES-001    Calibration bundle download    test_calibration_bundle_download

LOG-001 Log Text Export
    [Tags]    LOG-001    logs    files
    Run Regression Check    LOG-001    Log text export    test_log_download

SAFE-001 Zero Mask Command Path
    [Tags]    SAFE-001    output    safety
    Run Regression Check    SAFE-001    Zero-mask command path    test_zero_mask_command_path

SAFE-002 Repeated All Off Command Path
    [Tags]    SAFE-002    output    safety
    Run Regression Check    SAFE-002    Repeated all-off command path    test_repeated_all_off
