*** Settings ***
Documentation     Offline source-structure and optional Arduino CLI build checks for an optimization gate.
Resource          ../resources/source_build.resource
Suite Setup       Initialize Source Build Regression
Suite Teardown    Finalize Regression Profile
Force Tags        e-resistor    regression    source    build

*** Test Cases ***
SRC Gate Structure Checks
    [Tags]    source    gate
    Run Source Gate Checks

BUILD-001 Arduino CLI Compilation
    [Tags]    BUILD-001    build
    Run Arduino Build
