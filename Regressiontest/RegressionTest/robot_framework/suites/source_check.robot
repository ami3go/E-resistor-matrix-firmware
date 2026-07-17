*** Settings ***
Documentation     Offline firmware source-structure and optimization-gate checks. No firmware compiler is invoked.
Resource          ../resources/source_check.resource
Suite Setup       Initialize Source Check Regression
Suite Teardown    Finalize Regression Profile
Force Tags        e-resistor    regression    source    offline

*** Test Cases ***
SRC Gate Structure Checks
    [Tags]    source    gate
    Run Source Gate Checks
