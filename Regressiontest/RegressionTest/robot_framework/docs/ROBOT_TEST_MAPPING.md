# Robot Framework Test Mapping

## Runtime and protocol suite

| Regression ID | Robot test | Existing Python method | Profiles | Main evidence |
|---|---|---|---|---|
| NET-001 | TCP Reachability | `test_tcp_reachability` | read-only, safe-output, HIL | `events.jsonl`, `run.log` |
| HTTP-001 | HTTP Ping | `test_http_ping` | read-only, safe-output, HIL | `http_transcript.log` |
| HTTP-002 | State Schema And Readiness | `test_state_schema` | read-only, safe-output, HIL | `state_before.txt`, HTTP transcript |
| HTTP-003 | Required GUI Pages | `test_pages` | read-only, safe-output, HIL | HTTP transcript, metrics |
| SCPI-001 | Identity And Greeting | `test_scpi_identity` | read-only, safe-output, HIL | `scpi_transcript.log` |
| SCPI-002 | Status And State Consistency | `test_scpi_state_consistency` | read-only, safe-output, HIL | HTTP and SCPI transcripts |
| SCPI-003 | Calibration Table Integrity | `test_calibration` | read-only, safe-output, HIL | SCPI transcript |
| SCPI-004 | Undefined Header Error Queue | `test_scpi_error_queue` | read-only, safe-output, HIL | SCPI transcript |
| SCPI-005 | Oversized Line Recovery | `test_scpi_overflow_recovery` | read-only, safe-output, HIL | SCPI transcript |
| PERF-001 | HTTP Latency Sample | `test_http_latency` | read-only, safe-output, HIL | `metrics.csv` |
| PERF-002 | SCPI Latency Sample | `test_scpi_latency` | read-only, safe-output, HIL | `metrics.csv` |
| MEM-001 | Repeated State Heap Stability | `test_heap_stability` | read-only, safe-output, HIL | state snapshots, metrics |
| STRESS-001 | Concurrent HTTP And SCPI Polling | `test_concurrent_polling` | read-only, safe-output, HIL | protocol transcripts, metrics |
| FILES-001 | Calibration Bundle Download | `test_calibration_bundle_download` | read-only, safe-output, HIL | HTTP transcript |
| LOG-001 | Log Text Export | `test_log_download` | read-only, safe-output, HIL | HTTP transcript |

## Safe-output suite

| Regression ID | Robot test | Existing Python method | Safety level | Evidence |
|---|---|---|---|---|
| SAFE-001 | Zero Mask Command Path | `test_zero_mask_command_path` | Zero masks only | SCPI transcript, state-after |
| SAFE-002 | Repeated All Off Command Path | `test_repeated_all_off` | ALL:OFF only | SCPI transcript, latency metrics |

## Single-channel HIL suite

| Regression ID | Robot test | Existing Python method | Physical verification | Evidence |
|---|---|---|---|---|
| HIL-001 | COM Port And DMM Discovery | `test_hil_fixture_discovery` | Instrument identity and selected-channel calibration | hardware manifest, serial and DMM transcripts |
| HIL-002 | Fixture Safe State Precheck | `test_hil_safe_state_precheck` | DMM verifies open/OFF resistance | HIL metrics and DMM transcript |
| HIL-003 | Single Bit Physical Resistance Walk | `test_hil_single_bit_walk` | All selected resistor bits | `hil_measurements.csv` |
| HIL-004 | Combination Mask Physical Resistance | `test_hil_combination_masks` | Configured parallel combinations | `hil_measurements.csv` |
| HIL-005 | Repeated Physical Switching | `test_hil_repeated_switching` | Repeatability and cycling | HIL summary and metrics |
| HIL-006 | Serial Fault Log Inspection | `test_hil_serial_health` | RP2040 diagnostic stream | serial transcript and console log |
| HIL-007 | Final All Off Isolation Measurement | `test_hil_final_all_off` | DMM verifies final isolation | DMM transcript and HIL metrics |


## Runner-level evidence

| File | Purpose |
|---|---|
| `output.xml` | Robot machine-readable result model |
| `log.html` | Robot detailed keyword log |
| `report.html` | Robot high-level pass/fail report |
| `robot_events.jsonl` | Robot suite/test lifecycle and timing |
| `events.jsonl` | Contextual E-Resistor test and HIL events |
| `results.json` | Existing unified result schema |
| `test_coverage.*` | Requirement-to-test traceability |
| `evidence_manifest.sha256` | Integrity hashes for every evidence file |

## Gate 2 mappings

| Test ID | Robot method | Purpose |
|---|---|---|
| SCPI-006 | `test_scpi_target_calculation` | Validate dry-run target masks against host calibration math and verify outputs remain unchanged. |
| HIL-008 | `test_hil_serial_diagnostic` | Trigger and capture a deterministic structured USB serial event. |
