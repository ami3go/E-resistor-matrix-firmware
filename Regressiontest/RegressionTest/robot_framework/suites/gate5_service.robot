*** Settings ***
Documentation     Gate 5 HTTP/SCPI restructuring validation, compatibility, heap, streaming, and latency acceptance.
Resource          ../resources/common.resource
Suite Setup       Initialize Regression Profile    gate5_service
Suite Teardown    Finalize Regression Profile
Force Tags        e-resistor    regression    gate5    service    read-only

*** Test Cases ***
HTTP-002 State Schema And Gate Match
    [Tags]    HTTP-002    gate5    state    prerequisite
    Run Regression Check    HTTP-002    State schema and readiness    test_state_schema

G5-001 Browser Content And Flash-Backed Assets
    [Tags]    G5-001    gate5    http    browser    assets
    Run Regression Check    G5-001    Browser content and flash-backed assets    test_gate5_browser_assets

G5-002 API V1 Schema And Compatibility Aliases
    [Tags]    G5-002    gate5    http    api    compatibility
    Run Regression Check    G5-002    API v1 schema and compatibility aliases    test_gate5_api_v1_schema

G5-003 HTTP Mutation Method Policy
    [Tags]    G5-003    gate5    http    safety    methods
    Run Regression Check    G5-003    HTTP mutation method policy    test_gate5_http_mutation_methods

G5-004 SCPI Registry And Alias Compatibility
    [Tags]    G5-004    gate5    scpi    compatibility    registry
    Run Regression Check    G5-004    SCPI registry and alias compatibility    test_gate5_scpi_alias_compatibility

G5-005 Calibration Page Temporary Heap Reduction
    [Tags]    G5-005    gate5    http    memory    calibration
    Run Regression Check    G5-005    Calibration page temporary heap reduction    test_gate5_calibration_heap_reduction

G5-006 Thousand-Page Heap And State Latency
    [Tags]    G5-006    gate5    http    performance    memory    stress
    [Timeout]    30 minutes
    Run Regression Check    G5-006    Thousand-page heap and state latency    test_gate5_page_heap_and_state_latency
