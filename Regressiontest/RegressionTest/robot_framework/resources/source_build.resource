*** Settings ***
Resource    common.resource

*** Keywords ***
Initialize Source Build Regression
    Initialize Regression Profile    source_build

Run Source Gate Checks
    EResistor.Source Gate Checks Should Pass

Run Arduino Build
    EResistor.Arduino Build Should Pass
