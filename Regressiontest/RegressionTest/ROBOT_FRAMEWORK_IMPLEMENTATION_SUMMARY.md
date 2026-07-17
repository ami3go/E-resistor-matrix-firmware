# Robot Framework Implementation Summary

## Added profiles

| Profile | Suite | Robot tests |
|---|---|---:|
| Read only | `robot_framework/suites/read_only.robot` | 16 |
| Safe output | `robot_framework/suites/safe_output.robot` | 18 |
| Single-channel HIL | `robot_framework/suites/hil_single_channel.robot` | 24 |
| Source checks | `robot_framework/suites/source_check.robot` | 1 |

The suite directory contains 59 Robot test definitions. Profiles are normally executed separately, so repeated base tests are intentional.

## Reused implementation

Robot test cases call the existing methods in `e_resistor_regression.suite.RegressionSuite`. No SCPI, HTTP, DMM, serial, calculation, acceptance or cleanup logic was forked.

## Added diagnostics

- Robot `output.xml`, `log.html`, and `report.html`.
- Robot lifecycle events in `robot_events.jsonl`.
- Context fields in Python protocol and event logs: run ID, gate, profile, Robot test, regression test ID, operation ID.
- Explicit HTTP, SCPI and DMM error transcript records.
- SHA-256 evidence manifest.
- Incomplete-run marker removed only after successful finalization.

## Safety gates

- `safe_output` cannot start without explicit zero-output authorization.
- `hil_single_channel` cannot start without active-output authorization and the exact fixture-confirmation token.
- Active profiles issue best-effort all-off after every Robot test and at suite teardown.
- Existing HIL logic additionally verifies all channel masks and DMM isolation.

## Validation completed

- Python compilation passed.
- Python unit and package-layout tests passed: 36/36.
- Robot Framework dry run passed: 59/59 test definitions.
- A live offline `source_check` execution passed and produced Robot and legacy evidence without invoking a firmware compiler.
- A deliberately unreachable-device run produced 15 individual Robot failures and still finalized complete failure evidence.

Actual Ethernet/COM/DMM HIL execution remains to be run on the user's physical bench.

## Packaging convention

Package release **2.5.2** uses a descriptive, versioned ZIP filename while retaining the fixed internal root folder `RegressionTest/`. Future package releases must keep this internal folder name unchanged.
