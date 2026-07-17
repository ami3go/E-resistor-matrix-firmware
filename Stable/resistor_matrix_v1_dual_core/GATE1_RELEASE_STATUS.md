# E-Resistor G0 Review and Gate G1 Status

Date: 2026-07-16

## Input reviewed

`G0-hil_single_channel-20260716T123002Z.zip`

## G0 decision

**ACCEPTED — suitable as the optimization baseline.**

- Robot Framework: **22 passed, 0 failed, 0 skipped**.
- Suite duration: approximately **256 seconds**.
- All 16 selected-channel branch measurements passed the 1% error limit.
- All tested parallel combinations passed.
- Repeated switching passed 10/10 cycles.
- Heap decline after 100 state requests: **0 bytes**.
- Final all-off resistance was open/infinite.

### Key G0 metrics

| Metric | Result |
|---|---:|
| HTTP `/ping` p95 | 100.031 ms |
| HTTP `/state` p95 | 99.066 ms |
| SCPI `*IDN?` p95 | 217.749 ms |
| Concurrent HTTP state p95 | 129.747 ms |
| Concurrent SCPI state p95 | 232.570 ms |
| Single-bit apply p95 | 228.277 ms |
| Single-bit maximum absolute error | 0.644344% |
| Single-bit average absolute error | 0.061650% |
| Combination maximum absolute error | 0.007460% |
| Repeat switching maximum absolute error | 0.000521% |
| Repeatability standard deviation | 0.0000345% |

## G0 findings addressed in G1

1. Oversized SCPI input produced an overflow error followed by an undefined-header error from the remaining line tail.
2. The USB COM capture contained zero firmware lines, limiting internal troubleshooting.
3. Core 1 performed extensive formatted Serial logging and blocking `Serial.flush()` operations.
4. All-off callers could not reliably propagate or preserve physical shutdown failure.

## Gate G1 implementation

Firmware target: **v0.4.6**

- Removed direct `Serial.print`, `Serial.println`, and `Serial.flush` calls from Core 1 production paths.
- Added a fixed-size compact Core 1 event queue.
- Core 0 now formats Core 1 diagnostics as structured `EVT` records.
- Added compile-time firmware log levels.
- Changed `forceAllOff()` and `forceAllOffPhysical()` to return verified success/failure.
- HTTP, SCPI, startup, safe-state, and OTA paths now preserve all-off failures.
- Fixed SCPI overflow handling by discarding the rest of the invalid line until newline.
- Enabled a separate Core 1 stack.
- Added Core 1 maximum-loop, minimum-free-stack, event-count, and event-drop telemetry.
- Centralized the default device IP as `192.168.0.55`.
- Replaced Gate 1 channel/bit magic limits with `CHANNEL_COUNT` and `BIT_COUNT`.
- Removed obsolete split-from-monolithic-sketch comments.
- Added automated Gate 1 source checks.

## Offline validation

| Validation | Result |
|---|---:|
| Firmware Gate 1 source checks | **10/10 passed** |
| Regression Python unit/layout tests | **23/23 passed** |
| Robot Framework dry run | **56/56 passed** |
| Native G1 source profile | **8 passed, 0 failed, 1 device skip** |
| Core 1 production Serial calls | **0** |
| ZIP/root-layout checks | **Passed during release packaging** |

## Gate status

**G1 implementation status: SOURCE COMPLETE**

**G1 release status: HARDWARE VALIDATION REQUIRED**

Gate G1 must not be marked fully closed until the following pass on the real board:

1. Compile firmware v0.4.6 with the project's exact Arduino-Pico core and libraries.
2. Record firmware binary size and confirm growth is no more than 3%, unless justified.
3. Run G1 read-only regression.
4. Run G1 safe-output regression.
5. Run G1 single-channel HIL regression.
6. Confirm SCPI overflow produces one overflow error and the next `*IDN?` succeeds.
7. Confirm USB COM evidence contains structured `EVT` records.
8. Confirm `core1_event_drop_count=0`.
9. Confirm single-channel apply p95 is no worse than 5% above the G0 value of 228.277 ms.
10. Confirm all G0 functional and physical tests still pass.

## Included baseline

The updated regression package contains a machine-readable G0 baseline at:

`RegressionTest/baselines/G0_hil_single_channel_20260716T123002Z/results.json`

Use it through `ERESISTOR_BASELINE` or `--baseline` when running G1.
