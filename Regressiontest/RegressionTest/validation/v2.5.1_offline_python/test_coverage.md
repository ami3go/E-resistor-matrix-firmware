# Test Coverage Table

Coverage status is based on the current regression run. `NOT_RUN` means the test exists but the selected profile or command did not execute it. `PLANNED` and `MANUAL` are never counted as passed.

## Coverage summary

| PASS | FAIL | PARTIAL | SKIP | NOT_RUN | PLANNED | MANUAL |
|---:|---:|---:|---:|---:|---:|---:|
| 0 | 0 | 0 | 0 | 29 | 0 | 0 |

## Requirement coverage

| ID | Area | Requirement | Type | Implementation | Profile(s) | Test(s) | Status | Acceptance | Evidence |
|---|---|---|---|---|---|---|---:|---|---|
| COV-001 | Connectivity | Board TCP services are reachable | Automated | Implemented | read_only,safe_output,hil_single_channel | NET-001 | NOT_RUN | Required TCP ports accept connections within timeout | results.json, scpi_transcript.log |
| COV-002 | HTTP | HTTP health endpoint responds | Automated | Implemented | read_only,safe_output,hil_single_channel | HTTP-001 | NOT_RUN | HTTP response is successful and recognizable | http_transcript.log |
| COV-003 | HTTP | Runtime state schema and readiness are valid | Automated | Implemented | read_only,safe_output,hil_single_channel | HTTP-002 | NOT_RUN | State contains firmware, readiness, heap and eight channel records | state_before.txt, results.json |
| COV-004 | GUI | Required GUI pages remain available | Automated | Implemented | read_only,safe_output,hil_single_channel | HTTP-003 | NOT_RUN | Every required route returns HTTP 200 | http_transcript.log |
| COV-005 | SCPI | SCPI identity and greeting remain compatible | Automated | Implemented | read_only,safe_output,hil_single_channel | SCPI-001 | NOT_RUN | *IDN? identifies E-Resistor and connection greeting is valid | scpi_transcript.log |
| COV-006 | SCPI | SCPI state agrees with HTTP state | Automated | Implemented | read_only,safe_output,hil_single_channel | SCPI-002 | NOT_RUN | All eight masks and reported states are consistent | state_before.txt, scpi_transcript.log |
| COV-007 | Calibration | All channel calibration tables contain valid 16-bit data | Automated | Implemented | read_only,safe_output,hil_single_channel | SCPI-003 | NOT_RUN | CH1-CH8 each contain bits 0-15 with finite positive resistance | scpi_transcript.log, results.json |
| COV-008 | SCPI | Undefined command error handling remains correct | Automated | Implemented | read_only,safe_output,hil_single_channel | SCPI-004 | NOT_RUN | Invalid command enters and clears the expected error queue | scpi_transcript.log |
| COV-009 | SCPI | Oversized SCPI line is rejected and parser recovers | Automated | Implemented | read_only,safe_output,hil_single_channel | SCPI-005 | NOT_RUN | Oversized line is rejected and the next valid command succeeds | scpi_transcript.log |
| COV-010 | Performance | HTTP latency does not regress beyond gate limit | Automated | Implemented | read_only,safe_output,hil_single_channel | PERF-001 | NOT_RUN | Latency samples pass and baseline regression remains within configured percentage | metrics.csv, report.md |
| COV-011 | Performance | SCPI latency does not regress beyond gate limit | Automated | Implemented | read_only,safe_output,hil_single_channel | PERF-002 | NOT_RUN | Latency samples pass and baseline regression remains within configured percentage | metrics.csv, report.md |
| COV-012 | Memory | Repeated state requests do not cause excessive heap decline | Automated | Implemented | read_only,safe_output,hil_single_channel | MEM-001 | NOT_RUN | Heap decline remains below configured byte limit | metrics.csv, state_before.txt, state_after.txt |
| COV-013 | Stress | Concurrent HTTP and SCPI traffic remains stable | Automated | Implemented | read_only,safe_output,hil_single_channel | STRESS-001 | NOT_RUN | No protocol error and latency remains within gate limits | http_transcript.log, scpi_transcript.log |
| COV-014 | Files | Combined calibration bundle downloads successfully | Automated | Implemented | read_only,safe_output,hil_single_channel | FILES-001 | NOT_RUN | Bundle contains BEGIN markers for CH1-CH8 | http_transcript.log |
| COV-015 | Log | Firmware log exports as a text file | Automated | Implemented | read_only,safe_output,hil_single_channel | LOG-001 | NOT_RUN | Log endpoint returns a successful text response | http_transcript.log |
| COV-016 | Output safety | Zero-mask all-channel command path is safe | Automated | Implemented | safe_output | SAFE-001 | NOT_RUN | Command returns OK and every channel remains 0000 | scpi_transcript.log, state_after.txt |
| COV-017 | Output safety | Repeated ALL:OFF remains reliable | Automated | Implemented | safe_output | SAFE-002 | NOT_RUN | All requests return OK and final state is eight zero masks | scpi_transcript.log, metrics.csv |
| COV-018 | HIL fixture | USB COM and DMM are discovered and identified | HIL | Implemented | hil_single_channel | HIL-001 | NOT_RUN | Selected COM opens, DMM *IDN? matches and calibration is available | serial_console.log, dmm_transcript.log |
| COV-019 | HIL safety | Fixture starts with all outputs OFF and high isolation | HIL | Implemented | hil_single_channel | HIL-002 | NOT_RUN | All masks are 0000 and measured OFF resistance exceeds configured minimum | hil_measurements.csv, scpi_transcript.log |
| COV-020 | Physical accuracy | Every selected single resistor bit matches calibration | HIL | Implemented | hil_single_channel | HIL-003 | NOT_RUN | All selected bits stabilize and absolute error stays within configured limit | hil_measurements.csv, dmm_transcript.log |
| COV-021 | Physical accuracy | Selected parallel combinations match calculated resistance | HIL | Implemented | hil_single_channel | HIL-004 | NOT_RUN | Every configured mask stabilizes and remains within error limit | hil_measurements.csv |
| COV-022 | Repeatability | Repeated ON/OFF switching remains accurate and repeatable | HIL | Implemented | hil_single_channel | HIL-005 | NOT_RUN | All cycles pass accuracy, mask verification and repeatability limits | hil_measurements.csv, metrics.csv |
| COV-023 | Diagnostics | USB serial stream contains no fatal firmware fault patterns | HIL | Implemented | hil_single_channel | HIL-006 | NOT_RUN | No configured panic, assert, hardfault or queue-overflow pattern is detected | serial_console.log |
| COV-024 | HIL safety | Final ALL:OFF produces physical channel isolation | HIL | Implemented | hil_single_channel | HIL-007 | NOT_RUN | Final masks are zero and measured OFF resistance exceeds configured minimum | hil_measurements.csv, state_after.txt |
| COV-025 | Source quality | Gate-specific source expectations and legacy-pattern limits pass | Source | Implemented | read_only,safe_output,hil_single_channel | SRC-* | NOT_RUN | Every source expectation in the gate manifest passes | source_metrics.json, results.json |
| COV-026 | Build | Firmware compiles using the pinned Arduino CLI environment | Build | Implemented | read_only,safe_output,hil_single_channel | BUILD-001 | NOT_RUN | Compile return code is zero and gate warning/error limits pass | build.log, results.json |
| COV-027 | Numeric model | Numeric resistance model is equivalent to the previous implementation | Combined | Implemented | read_only,hil_single_channel | SCPI-003,SCPI-006,HIL-003,HIL-004 | NOT_RUN | Calibration parsing and physical bit/combination results pass; host oracle comparison is still recommended | scpi_transcript.log, hil_measurements.csv |
| COV-041 | Target search | Read-only target-mask calculation matches the calibrated host oracle and never changes outputs | Automated | Implemented | read_only,safe_output,hil_single_channel | SCPI-006 | NOT_RUN | Multiple targets return valid masks, resistance/error fields agree with host calculation, deadlines pass, and masks remain unchanged | scpi_transcript.log, metrics.csv, state_before.txt, state_after.txt |
| COV-042 | Diagnostics | A deterministic firmware event is observable on the configured RP2040 USB COM port | HIL | Implemented | hil_single_channel | HIL-008 | NOT_RUN | SYST:DIAG:SERIAL? returns OK and the matching structured SERIAL_TEST event is captured | serial_console.log, scpi_transcript.log |
