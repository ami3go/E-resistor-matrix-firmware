# E-Resistor Regression Report

- Gate: **G0**
- Profile: **safe_output**
- Host: `192.168.0.55`
- Overall: **FAIL**
- Passed: 12
- Failed: 6
- Skipped: 0

## Test results

| ID | Test | Status | Duration, ms | Message |
|---|---|---:|---:|---|
| NET-001 | TCP reachability | PASS | 80.55 | HTTP and SCPI ports reachable |
| HTTP-001 | HTTP ping | PASS | 641.52 | HTTP 200, body='pong' |
| HTTP-002 | State schema and readiness | FAIL | 115.19 | {"channel_count": 0, "missing": [], "readiness_failures": []} |
| HTTP-003 | Required GUI pages | PASS | 2024.71 | 9 pages valid |
| SCPI-001 | SCPI identity and greeting | PASS | 901.78 | OpenBench,E-Resistor,E66178758B3E742A,0.4.4 |
| SCPI-002 | SCPI status and state consistency | FAIL | 875.94 | mismatches=[] |
| SCPI-003 | Calibration table integrity | PASS | 484.87 | 8 x 16 table valid |
| SCPI-004 | Undefined-header error queue | PASS | 1201.46 | invalid='ERR,-113,"Undefined header"', error='-113,"Undefined header"', cleared='0,"No error"' |
| SCPI-005 | Oversized-line recovery | PASS | 737.32 | recovered=True, overflow='ERR,-350,"Input buffer overflow"\r\nERR,-113,"Undefined header"' |
| PERF-001 | HTTP latency sample | PASS | 5418.76 | 30 ping and state samples |
| PERF-002 | SCPI latency sample | PASS | 7354.12 | 30 samples |
| MEM-001 | Repeated-state heap stability | PASS | 8436.93 | heap decline 0 bytes; limit 2048 |
| STRESS-001 | Concurrent HTTP and SCPI polling | FAIL | 4017.51 | AssertionError: Incomplete SCPI state response |
| FILES-001 | Calibration bundle download | PASS | 89.16 | HTTP 200, 2661 bytes |
| LOG-001 | Log text export | PASS | 61.03 | HTTP 200, content-type=text/plain; charset=utf-8 |
| SAFE-001 | Zero-mask command path | FAIL | 757.90 | apply='OK' |
| SAFE-002 | Repeated all-off command path | FAIL | 4948.18 | responses=['OK'] |
| SAFE-999 | Verified final all-off cleanup | FAIL | 2599.20 | Unable to verify ALL:OFF after 3 attempt(s) |

## Test coverage

The complete requirement traceability table is written to `test_coverage.md`, `test_coverage.csv`, and `test_coverage.json`.

| PASS | FAIL | PARTIAL | SKIP | NOT_RUN | PLANNED | MANUAL |
|---:|---:|---:|---:|---:|---:|---:|
| 12 | 4 | 0 | 0 | 9 | 0 | 0 |

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
| COV-024A | Final cleanup is retried and all software masks are verified OFF | FAIL | SAFE-999 |
| COV-025 | Gate-specific source expectations and legacy-pattern limits pass | NOT_RUN | SRC-* |
| COV-026 | Firmware compiles using the pinned Arduino CLI environment | NOT_RUN | BUILD-001 |
