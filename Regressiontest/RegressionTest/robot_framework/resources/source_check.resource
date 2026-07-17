*** Settings ***
Resource    common.resource

*** Keywords ***
Initialize Source Check Regression
    Initialize Regression Profile    source_check

Run Source Gate Checks
    EResistor.Source Gate Checks Should Pass
