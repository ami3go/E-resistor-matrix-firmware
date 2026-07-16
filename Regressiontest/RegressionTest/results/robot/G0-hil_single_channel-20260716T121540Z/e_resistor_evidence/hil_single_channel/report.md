# E-Resistor Regression Report

- Gate: **G0**
- Profile: **hil_single_channel**
- Host: `192.168.0.55`
- Overall: **FAIL**
- Passed: 12
- Failed: 5
- Skipped: 6

## Test results

| ID | Test | Status | Duration, ms | Message |
|---|---|---:|---:|---|
| NET-001 | TCP reachability | PASS | 95.32 | HTTP and SCPI ports reachable |
| HTTP-001 | HTTP ping | PASS | 130.32 | HTTP 200, body='pong' |
| HTTP-002 | State schema and readiness | FAIL | 110.28 | {"channel_count": 0, "missing": [], "readiness_failures": []} |
| HTTP-003 | Required GUI pages | PASS | 1465.88 | 9 pages valid |
| SCPI-001 | SCPI identity and greeting | PASS | 696.78 | OpenBench,E-Resistor,E66178758B3E742A,0.4.4 |
| SCPI-002 | SCPI status and state consistency | FAIL | 865.46 | mismatches=[] |
| SCPI-003 | Calibration table integrity | PASS | 525.83 | 8 x 16 table valid |
| SCPI-004 | Undefined-header error queue | PASS | 1146.95 | invalid='ERR,-113,"Undefined header"', error='-113,"Undefined header"', cleared='0,"No error"' |
| SCPI-005 | Oversized-line recovery | PASS | 670.57 | recovered=True, overflow='ERR,-350,"Input buffer overflow"\r\nERR,-113,"Undefined header"' |
| PERF-001 | HTTP latency sample | PASS | 5162.15 | 30 ping and state samples |
| PERF-002 | SCPI latency sample | PASS | 6956.40 | 30 samples |
| MEM-001 | Repeated-state heap stability | PASS | 7673.27 | heap decline 0 bytes; limit 2048 |
| STRESS-001 | Concurrent HTTP and SCPI polling | FAIL | 4245.33 | AssertionError: Incomplete SCPI state response |
| FILES-001 | Calibration bundle download | PASS | 93.39 | HTTP 200, 2661 bytes |
| LOG-001 | Log text export | PASS | 76.05 | HTTP 200, content-type=text/plain; charset=utf-8 |
| HIL-001 | COM port and DMM discovery | FAIL | 724.74 | RuntimeError: ALL:OFF verification failed: {} |
| HIL-002 | Fixture safe-state precheck | SKIP | 0.01 | HIL fixture was not initialized or failed its safety precheck |
| HIL-003 | Single-bit physical resistance walk | SKIP | 0.01 | HIL fixture was not initialized or failed its safety precheck |
| HIL-004 | Combination-mask physical resistance | SKIP | 0.01 | HIL fixture was not initialized or failed its safety precheck |
| HIL-005 | Repeated physical switching | SKIP | 0.00 | HIL fixture was not initialized or failed its safety precheck |
| HIL-006 | Serial fault-log inspection | SKIP | 0.01 | HIL fixture was not initialized or failed its safety precheck |
| HIL-007 | Final all-off isolation measurement | SKIP | 0.01 | HIL fixture was not initialized or failed its safety precheck |
| SAFE-999 | Verified final all-off cleanup | FAIL | 2543.02 | Unable to verify ALL:OFF after 3 attempt(s) |

## Test coverage

The complete requirement traceability table is written to `test_coverage.md`, `test_coverage.csv`, and `test_coverage.json`.

| PASS | FAIL | PARTIAL | SKIP | NOT_RUN | PLANNED | MANUAL |
|---:|---:|---:|---:|---:|---:|---:|
| 12 | 5 | 0 | 6 | 2 | 0 | 0 |

### Coverage gaps and exceptions

| ID | Requirement | Status | Test(s) |
|---|---|---:|---|
| COV-003 | Runtime state schema and readiness are valid | FAIL | HTTP-002 |
| COV-006 | SCPI state agrees with HTTP state | FAIL | SCPI-002 |
| COV-013 | Concurrent HTTP and SCPI traffic remains stable | FAIL | STRESS-001 |
| COV-018 | USB COM and DMM are discovered and identified | FAIL | HIL-001 |
| COV-019 | Fixture starts with all outputs OFF and high isolation | SKIP | HIL-002 |
| COV-020 | Every selected single resistor bit matches calibration | SKIP | HIL-003 |
| COV-021 | Selected parallel combinations match calculated resistance | SKIP | HIL-004 |
| COV-022 | Repeated ON/OFF switching remains accurate and repeatable | SKIP | HIL-005 |
| COV-023 | USB serial stream contains no fatal firmware fault patterns | SKIP | HIL-006 |
| COV-024 | Final ALL:OFF produces physical channel isolation | SKIP | HIL-007 |
| COV-024A | Final cleanup is retried and all software masks are verified OFF | FAIL | SAFE-999 |
| COV-025 | Gate-specific source expectations and legacy-pattern limits pass | NOT_RUN | SRC-* |
| COV-026 | Firmware compiles using the pinned Arduino CLI environment | NOT_RUN | BUILD-001 |
