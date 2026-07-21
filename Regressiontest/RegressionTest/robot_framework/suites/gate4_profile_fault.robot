*** Settings ***
Documentation     Gate 4 test-image fault injection after the global clear phase. Run only after production G4 regression and reflash production firmware afterward.
Resource          ../resources/common.resource
Suite Setup       Initialize Regression Profile    gate4_profile_fault
Suite Teardown    Finalize Regression Profile
Test Teardown     Safety Cleanup
Force Tags        e-resistor    regression    gate4    profile    fault-injection    single-channel

*** Test Cases ***
HTTP-002 State Schema And Gate Match
    [Tags]    HTTP-002    state    prerequisite
    Run Regression Check    HTTP-002    State schema and readiness    test_state_schema

CAL-001 Saved Calibration Presence
    [Tags]    CAL-001    calibration    persistence    prerequisite
    Run Regression Check    CAL-001    Saved calibration presence    test_calibration_storage_presence

HIL-001 COM Port And DMM Discovery
    [Tags]    HIL-001    fixture    dmm    serial    prerequisite
    [Timeout]    2 minutes
    Run Regression Check    HIL-001    COM port and DMM discovery    test_hil_fixture_discovery

HIL-002 Fixture Safe State Precheck
    [Tags]    HIL-002    safety    dmm    prerequisite
    [Timeout]    2 minutes
    Run Regression Check    HIL-002    Fixture safe-state precheck    test_hil_safe_state_precheck

G3-FI-001 Fault-Injection Build Detection
    [Tags]    G3-FI-001    test-mode    prerequisite
    Run Regression Check    G3-FI-001    Fault-injection build detection    test_gate3_test_mode

G4-FI-001 Failure After Global Clear Leaves All Outputs Off
    [Tags]    G4-FI-001    gate4    profile    fault-injection    dmm    safety
    [Timeout]    5 minutes
    Run Regression Check    G4-FI-001    Failure after global clear leaves all outputs off    test_gate4_profile_failure_after_clear

HIL-007 Final All Off Isolation Measurement
    [Tags]    HIL-007    safety    dmm
    [Timeout]    2 minutes
    Run Regression Check    HIL-007    Final all-off isolation measurement    test_hil_final_all_off
