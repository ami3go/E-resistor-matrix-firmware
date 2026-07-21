*** Settings ***
Documentation     Gate 3 test-build fault injection proving queued-generation and timed-out commands are rejected before physical actuation. Run only after normal G3 regression and reflash production firmware afterward.
Resource          ../resources/common.resource
Suite Setup       Initialize Regression Profile    gate3_transport_fault
Suite Teardown    Finalize Regression Profile
Test Teardown     Safety Cleanup
Force Tags        e-resistor    regression    gate3    fault-injection    single-channel

*** Test Cases ***
HTTP-002 State Schema And Gate Match
    [Tags]    HTTP-002    state    prerequisite
    Run Regression Check    HTTP-002    State schema and readiness    test_state_schema

HIL-001 COM Port And DMM Discovery
    [Tags]    HIL-001    fixture    dmm    serial    prerequisite
    [Timeout]    2 minutes
    Run Regression Check    HIL-001    COM port and DMM discovery    test_hil_fixture_discovery

HIL-002 Fixture Safe State Precheck
    [Tags]    HIL-002    safety    dmm    prerequisite
    [Timeout]    2 minutes
    Run Regression Check    HIL-002    Fixture safe-state precheck    test_hil_safe_state_precheck

G3-FI-001 Fault-Injection Build Detection
    [Tags]    G3-FI-001    gate3    test-mode
    Run Regression Check    G3-FI-001    Fault-injection build detection    test_gate3_test_mode

G3-FI-002 Queued Generation Invalidation Has No Ghost Actuation
    [Tags]    G3-FI-002    gate3    generation    queued-command    dmm    safety
    [Timeout]    5 minutes
    Run Regression Check    G3-FI-002    Queued generation invalidation has no ghost actuation    test_gate3_invalidated_command_no_ghost_actuation

G3-FI-003 Timed-Out Command Has No Ghost Actuation
    [Tags]    G3-FI-003    gate3    timeout    deadline    generation    dmm    safety
    [Timeout]    5 minutes
    Run Regression Check    G3-FI-003    Timed-out command has no ghost actuation    test_gate3_timeout_no_ghost_actuation

HIL-007 Final All Off Isolation Measurement
    [Tags]    HIL-007    safety    dmm
    [Timeout]    2 minutes
    Run Regression Check    HIL-007    Final all-off isolation measurement    test_hil_final_all_off
