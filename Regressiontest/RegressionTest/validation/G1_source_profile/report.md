# E-Resistor Regression Report

- Gate: **G1**
- Profile: **read_only**
- Host: `192.168.0.55`
- Overall: **PASS**
- Passed: 8
- Failed: 0
- Skipped: 1

## Test results

| ID | Test | Status | Duration, ms | Message |
|---|---|---:|---:|---|
| SRC-001 | Core 1 Serial.flush limit | PASS | 0.00 | actual=0, expected='<= 0' |
| SRC-002 | Core 1 Serial call limit | PASS | 0.00 | actual=0, expected='<= 0' |
| SRC-003 | Required source marker: core1_separate_stack | PASS | 0.00 | actual=True, expected=True |
| SRC-004 | Required source marker: scpiDiscardUntilNewline | PASS | 0.00 | actual=True, expected=True |
| SRC-005 | Required source marker: struct Core1Event | PASS | 0.00 | actual=True, expected=True |
| SRC-006 | Required source marker: bool forceAllOff | PASS | 0.00 | actual=True, expected=True |
| SRC-007 | Required source marker: DEFAULT_DEVICE_IP_TEXT | PASS | 0.00 | actual=True, expected=True |
| SRC-008 | Forbidden source marker: Split from rp2040_w5500_resistor_matrix_v1_safety_logic.ino | PASS | 0.00 | actual=False, expected=False |
| DEVICE-000 | Hardware regression | SKIP | 0.00 | disabled by --skip-device |

## Test coverage

The complete requirement traceability table is written to `test_coverage.md`, `test_coverage.csv`, and `test_coverage.json`.

| PASS | FAIL | PARTIAL | SKIP | NOT_RUN | PLANNED | MANUAL |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 0 | 0 | 0 | 25 | 0 | 0 |

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
| COV-026 | Firmware compiles using the pinned Arduino CLI environment | NOT_RUN | BUILD-001 |

## Source metrics

- Files: 29
- Lines: 10257
- `app.h` lines: 1130
- `http_handlers.cpp` lines: 2302
- `String` tokens: 422
- Core-related Serial calls: 0
- State-changing GET routes: /set, /toggle_bit, /alloff, /profile_apply, /profile_delete, /target_apply, /factory_reset
