*** Settings ***
Documentation     Full read-only regression plus one-channel physical verification using SCPI, RP2040 USB COM logs, and a USB/VISA DMM.
Resource          ../resources/common.resource
Suite Setup       Initialize Regression Profile    hil_single_channel
Suite Teardown    Finalize Regression Profile
Test Teardown     Safety Cleanup
Force Tags        e-resistor    regression    hil    single-channel

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

HIL-001 COM Port And DMM Discovery
    [Tags]    HIL-001    fixture    dmm    serial
    [Timeout]    2 minutes
    Run Regression Check    HIL-001    COM port and DMM discovery    test_hil_fixture_discovery

HIL-002 Fixture Safe State Precheck
    [Tags]    HIL-002    safety    dmm
    [Timeout]    2 minutes
    Run Regression Check    HIL-002    Fixture safe-state precheck    test_hil_safe_state_precheck

HIL-003 Single Bit Physical Resistance Walk
    [Tags]    HIL-003    resistance    dmm    active-output
    [Timeout]    15 minutes
    Run Regression Check    HIL-003    Single-bit physical resistance walk    test_hil_single_bit_walk

HIL-004 Combination Mask Physical Resistance
    [Tags]    HIL-004    resistance    dmm    active-output
    [Timeout]    10 minutes
    Run Regression Check    HIL-004    Combination-mask physical resistance    test_hil_combination_masks

HIL-005 Repeated Physical Switching
    [Tags]    HIL-005    repeatability    dmm    active-output
    [Timeout]    15 minutes
    Run Regression Check    HIL-005    Repeated physical switching    test_hil_repeated_switching

HIL-006 Serial Fault Log Inspection
    [Tags]    HIL-006    serial    diagnostics
    Run Regression Check    HIL-006    Serial fault-log inspection    test_hil_serial_health

HIL-007 Final All Off Isolation Measurement
    [Tags]    HIL-007    safety    dmm
    [Timeout]    2 minutes
    Run Regression Check    HIL-007    Final all-off isolation measurement    test_hil_final_all_off
