# Gate 2 Release Status

- Regression package: **2.5.2**
- Firmware target: **0.5.0**
- Internal ZIP root: exactly `RegressionTest/`

## Harness correction release v2.5.2

- Restored optional `0x` mask parsing for HTTP and SCPI state.
- Added bounded `STATE?` retry and raw-response evidence.
- Added firmware/gate mismatch protection before active HIL.
- Corrected manifest finalization order and added automatic verification.
- Strengthened log-export validation.

## Implemented coverage

- Numeric indexed 8 × 16 calibration model source checks.
- Legacy text-runtime model and duplicate-default detection.
- Read-only `CH<n>:TARGET:CALC?` validation against the downloaded calibration table.
- Verification that dry-run calculations never change any output mask.
- Candidate count, firmware elapsed time, SCPI latency, timeout, and cancellation telemetry.
- Deterministic USB serial event test through `SYST:DIAG:SERIAL?`.
- Robot Framework mappings for SCPI-006 and HIL-008.
- Accepted Gate 1 HIL baseline and review.

## Pending gate-exit evidence

- Successful external firmware compilation and image-size report from the firmware repository.
- G2 read-only, safe-output, and single-channel HIL runs on the real bench.
- Structured Core 1 events and SERIAL_TEST event captured with zero drops.
- Target-search timing measured on the RP2040.
- No physical-accuracy, latency, memory, or safety regression from Gate 1.
