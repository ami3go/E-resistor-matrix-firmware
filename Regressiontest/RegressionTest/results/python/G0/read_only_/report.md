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
| NET-001 | TCP reachability | PASS | 83.94 | HTTP and SCPI ports reachable |
| HTTP-001 | HTTP ping | PASS | 212.35 | HTTP 200, body='pong' |
| HTTP-002 | State schema and readiness | FAIL | 76.04 | {"channel_count": 0, "missing": [], "readiness_failures": []} |
| HTTP-003 | Required GUI pages | PASS | 1395.31 | 9 pages valid |
| SCPI-001 | SCPI identity and greeting | PASS | 736.51 | OpenBench,E-Resistor,E66178758B3E742A,0.4.4 |
| SCPI-002 | SCPI status and state consistency | FAIL | 748.60 | mismatches=[] |
| SCPI-003 | Calibration table integrity | PASS | 519.94 | 8 x 16 table valid |
| SCPI-004 | Undefined-header error queue | PASS | 1126.67 | invalid='ERR,-113,"Undefined header"', error='-113,"Undefined header"', cleared='0,"No error"' |
| SCPI-005 | Oversized-line recovery | PASS | 709.60 | recovered=True, overflow='ERR,-350,"Input buffer overflow"\r\nERR,-113,"Undefined header"' |
| PERF-001 | HTTP latency sample | PASS | 4876.56 | 30 ping and state samples |
| PERF-002 | SCPI latency sample | PASS | 7208.79 | 30 samples |
| MEM-001 | Repeated-state heap stability | PASS | 8396.97 | heap decline 0 bytes; limit 2048 |
| STRESS-001 | Concurrent HTTP and SCPI polling | FAIL | 3950.36 | AssertionError: Incomplete SCPI state response |
| FILES-001 | Calibration bundle download | PASS | 122.94 | HTTP 200, 2661 bytes |
| LOG-001 | Log text export | PASS | 52.20 | HTTP 200, content-type=text/plain; charset=utf-8 |

## Test coverage

The complete requirement traceability table is written to `test_coverage.md`, `test_coverage.csv`, and `test_coverage.json`.

| PASS | FAIL | PARTIAL | SKIP | NOT_RUN | PLANNED | MANUAL |
|---:|---:|---:|---:|---:|---:|---:|
| 12 | 3 | 0 | 0 | 10 | 0 | 0 |

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
| COV-024A | Final cleanup is retried and all software masks are verified OFF | NOT_RUN | SAFE-999 |
| COV-025 | Gate-specific source expectations and legacy-pattern limits pass | NOT_RUN | SRC-* |
| COV-026 | Firmware compiles using the pinned Arduino CLI environment | NOT_RUN | BUILD-001 |
