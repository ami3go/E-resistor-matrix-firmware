# E-Resistor Regression Report

- Gate: **G2**
- Profile: **read_only**
- Host: `192.168.0.55`
- Overall: **PASS**
- Passed: 0
- Failed: 0
- Skipped: 1

## Test results

| ID | Test | Status | Duration, ms | Message |
|---|---|---:|---:|---|
| DEVICE-000 | Hardware regression | SKIP | 0.00 | disabled by --skip-device |

## Test coverage

The complete requirement traceability table is written to `test_coverage.md`, `test_coverage.csv`, and `test_coverage.json`.

| PASS | FAIL | PARTIAL | SKIP | NOT_RUN | PLANNED | MANUAL |
|---:|---:|---:|---:|---:|---:|---:|
| 0 | 0 | 0 | 0 | 29 | 0 | 0 |

### Coverage gaps and exceptions

| ID | Requirement | Status | Test(s) |
|---|---|---:|---|
| COV-001 | Board TCP services are reachable | NOT_RUN | NET-001 |
| COV-002 | HTTP health endpoint responds | NOT_RUN | HTTP-001 |
| COV-003 | Runtime state schema and readiness are valid | NOT_RUN | HTTP-002 |
| COV-004 | Required GUI pages remain available | NOT_RUN | HTTP-003 |
| COV-005 | SCPI identity and greeting remain compatible | NOT_RUN | SCPI-001 |
| COV-006 | SCPI state agrees with HTTP state | NOT_RUN | SCPI-002 |
| COV-007 | All channel calibration tables contain valid 16-bit data | NOT_RUN | SCPI-003 |
| COV-008 | Undefined command error handling remains correct | NOT_RUN | SCPI-004 |
| COV-009 | Oversized SCPI line is rejected and parser recovers | NOT_RUN | SCPI-005 |
| COV-010 | HTTP latency does not regress beyond gate limit | NOT_RUN | PERF-001 |
| COV-011 | SCPI latency does not regress beyond gate limit | NOT_RUN | PERF-002 |
| COV-012 | Repeated state requests do not cause excessive heap decline | NOT_RUN | MEM-001 |
| COV-013 | Concurrent HTTP and SCPI traffic remains stable | NOT_RUN | STRESS-001 |
| COV-014 | Combined calibration bundle downloads successfully | NOT_RUN | FILES-001 |
| COV-015 | Firmware log exports as a text file | NOT_RUN | LOG-001 |
| COV-016 | Zero-mask all-channel command path is safe | NOT_RUN | SAFE-001 |
| COV-017 | Repeated ALL:OFF remains reliable | NOT_RUN | SAFE-002 |
| COV-018 | USB COM and DMM are discovered and identified | NOT_RUN | HIL-001 |
| COV-019 | Fixture starts with all outputs OFF and high isolation | NOT_RUN | HIL-002 |
| COV-020 | Every selected single resistor bit matches calibration | NOT_RUN | HIL-003 |
| COV-021 | Selected parallel combinations match calculated resistance | NOT_RUN | HIL-004 |
| COV-022 | Repeated ON/OFF switching remains accurate and repeatable | NOT_RUN | HIL-005 |
| COV-023 | USB serial stream contains no fatal firmware fault patterns | NOT_RUN | HIL-006 |
| COV-024 | Final ALL:OFF produces physical channel isolation | NOT_RUN | HIL-007 |
| COV-025 | Gate-specific source expectations and legacy-pattern limits pass | NOT_RUN | SRC-* |
| COV-026 | Firmware compiles using the pinned Arduino CLI environment | NOT_RUN | BUILD-001 |
| COV-027 | Numeric resistance model is equivalent to the previous implementation | NOT_RUN | SCPI-003,SCPI-006,HIL-003,HIL-004 |
| COV-041 | Read-only target-mask calculation matches the calibrated host oracle and never changes outputs | NOT_RUN | SCPI-006 |
| COV-042 | A deterministic firmware event is observable on the configured RP2040 USB COM port | NOT_RUN | HIL-008 |
