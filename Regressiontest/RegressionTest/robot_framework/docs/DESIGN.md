# Robot Framework Adapter Design

## Goals

1. Keep one implementation of SCPI, HTTP, serial and DMM behavior.
2. Show each regression ID as an individual Robot test.
3. Preserve the existing JSON, CSV, Markdown and transcript evidence.
4. Add Robot's standard interactive HTML report and log.
5. Keep active-output tests opt-in and fail closed.
6. Keep the suite usable on Windows and Linux.

## Layers

```text
Robot .robot test case
        │
        ▼
common.resource keyword
        │
        ▼
EResistorRobotLibrary
        │
        ▼
existing RegressionSuite method
        │
        ├── HttpClient
        ├── ScpiClient
        ├── SerialMonitor
        └── VisaDmm
```

## Library scope

The custom library uses Robot `SUITE` scope. This is required because HIL tests share:

- One background serial monitor.
- One VISA DMM session.
- The selected-channel calibration table.
- Accumulated HIL measurements.
- One evidence directory and run ID.

## Status conversion

| Python result | Robot behavior |
|---|---|
| PASS | Keyword succeeds |
| FAIL | Keyword raises an assertion with the regression ID and message |
| SKIP | Robot's built-in skip mechanism is used |

The result is stored before Robot status is raised, so final coverage and evidence remain available after failures.

## Cleanup

Robot `Test Teardown` invokes `Safety Cleanup After Test`. It issues best-effort `ALL:OFF` for every active profile. Robot `Suite Teardown` repeats all-off, captures final state, closes DMM and serial resources, and writes reports.

The HIL methods themselves also perform verified all-off between physical measurements. The multiple cleanup layers are deliberate.

## Log correlation

The adapter adds these fields to Python event and transcript records:

- `run_id`
- `gate`
- `profile`
- `test_id`
- `robot_test_name`
- `operation_id`

This makes SCPI, HTTP, DMM, serial and result records searchable by one operation ID.

## Baseline and coverage

The finalizer calls the same baseline-comparison and coverage modules used by the Python CLI. Robot runs therefore enforce the same latency/error regression policy and produce the same requirement coverage states.

## Future profiles

The folder reserves the same future extensions as the Python harness:

- `active_output`
- `storage`
- `ota`
- `watchdog`

They should be added as separate `.robot` suites only when the firmware test hooks and safety fixture exist. They must not be hidden inside the current HIL suite.
