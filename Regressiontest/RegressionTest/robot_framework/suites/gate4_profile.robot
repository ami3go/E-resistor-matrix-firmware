*** Settings ***
Documentation     Gate 4 safe zero-profile stress and performance comparison against the corrected Gate 3 baseline.
Resource          ../resources/common.resource
Suite Setup       Initialize Regression Profile    gate4_profile
Suite Teardown    Finalize Regression Profile
Test Teardown     Safety Cleanup
Force Tags        e-resistor    regression    gate4    profile    safe-output

*** Test Cases ***
HTTP-002 State Schema And Gate Match
    [Tags]    HTTP-002    state    prerequisite
    Run Regression Check    HTTP-002    State schema and readiness    test_state_schema

CAL-001 Saved Calibration Presence
    [Tags]    CAL-001    calibration    persistence    prerequisite
    Run Regression Check    CAL-001    Saved calibration presence    test_calibration_storage_presence

G3-001 Core Transport Diagnostics
    [Tags]    G3-001    gate3    transport    prerequisite
    Run Regression Check    G3-001    Core transport diagnostics    test_core_transport_diagnostics

G4-001 Gate 4 Profile Diagnostics
    [Tags]    G4-001    gate4    profile    diagnostics
    Run Regression Check    G4-001    Gate 4 profile diagnostics    test_gate4_profile_diagnostics

G4-002 Thousand-Cycle Two-Phase Profile Stress
    [Tags]    G4-002    gate4    profile    performance    stress    safety
    [Timeout]    30 minutes
    Run Regression Check    G4-002    Thousand-cycle two-phase profile stress    test_gate4_zero_profile_stress

SCPI-002 Final State Consistency
    [Tags]    SCPI-002    state    safety
    Run Regression Check    SCPI-002    SCPI status and state consistency    test_scpi_state_consistency
