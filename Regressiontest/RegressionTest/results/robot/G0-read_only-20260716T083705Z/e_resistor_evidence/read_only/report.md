# E-Resistor Regression Report

- Gate: **G0**
- Profile: **read_only**
- Host: `192.168.0.55`
- Overall: **FAIL**
- Passed: 12
- Failed: 3
- Skipped: 0

## Test results

| ID | Test | Status | Duration, ms | Message |
|---|---|---:|---:|---|
| NET-001 | TCP reachability | PASS | 81.28 | HTTP and SCPI ports reachable |
| HTTP-001 | HTTP ping | PASS | 220.92 | HTTP 200, body='pong' |
| HTTP-002 | State schema and readiness | FAIL | 102.58 | {"channel_count": 0, "missing": [], "readiness_failures": []} |
| HTTP-003 | Required GUI pages | PASS | 1360.25 | 9 pages valid |
| SCPI-001 | SCPI identity and greeting | PASS | 684.23 | OpenBench,E-Resistor,E66178758B3E742A,0.4.4 |
| SCPI-002 | SCPI status and state consistency | FAIL | 774.96 | mismatches=[] |
| SCPI-003 | Calibration table integrity | PASS | 538.17 | 8 x 16 table valid |
| SCPI-004 | Undefined-header error queue | PASS | 1109.30 | invalid='ERR,-113,"Undefined header"', error='-113,"Undefined header"', cleared='0,"No error"' |
| SCPI-005 | Oversized-line recovery | PASS | 723.74 | recovered=True, overflow='ERR,-350,"Input buffer overflow"\r\nERR,-113,"Undefined header"' |
| PERF-001 | HTTP latency sample | PASS | 4282.22 | 30 ping and state samples |
| PERF-002 | SCPI latency sample | PASS | 6806.51 | 30 samples |
| MEM-001 | Repeated-state heap stability | PASS | 7722.98 | heap decline 0 bytes; limit 2048 |
| STRESS-001 | Concurrent HTTP and SCPI polling | FAIL | 4127.47 | AssertionError: Incomplete SCPI state response |
| FILES-001 | Calibration bundle download | PASS | 96.62 | HTTP 200, 2661 bytes |
| LOG-001 | Log text export | PASS | 114.47 | HTTP 200, content-type=text/plain; charset=utf-8 |

## Test coverage

The complete requirement traceability table is written to `test_coverage.md`, `test_coverage.csv`, and `test_coverage.json`.

| PASS | FAIL | PARTIAL | SKIP | NOT_RUN | PLANNED | MANUAL |
|---:|---:|---:|---:|---:|---:|---:|
| 12 | 3 | 0 | 0 | 9 | 0 | 0 |

### Coverage gaps and exceptions

| ID | Requirement | Status | Test(s) |
|---|---|---:|---|
| COV-003 | Runtime state schema and readiness are valid | FAIL | HTTP-002 |
| COV-006 | SCPI state agrees with HTTP state | FAIL | SCPI-002 |
| COV-013 | Concurrent HTTP and SCPI traffic remains stable | FAIL | STRESS-001 |
| COV-018 | USB COM and DMM are discovered and identified | NOT_RUN | HIL-001 |
| COV-019 | Fixture starts with all outputs OFF and high isolation | NOT_RUN | HIL-002 |
| COV-020 | Every selected single resistor bit matches calibration | NOT_RUN | HIL-003 |
| COV-021 | Selected parallel combinations match calculated resistance | NOT_RUN | HIL-004 |
| COV-022 | Repeated ON/OFF switching remains accurate and repeatable | NOT_RUN | HIL-005 |
| COV-023 | USB serial stream contains no fatal firmware fault patterns | NOT_RUN | HIL-006 |
| COV-024 | Final ALL:OFF produces physical channel isolation | NOT_RUN | HIL-007 |
| COV-025 | Gate-specific source expectations and legacy-pattern limits pass | NOT_RUN | SRC-* |
| COV-026 | Firmware compiles using the pinned Arduino CLI environment | NOT_RUN | BUILD-001 |
