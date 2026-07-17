# Gate 1 Hardware Regression Review

The uploaded archive was named `G0-hil_single_channel-20260717T103129Z`, but the device state and SCPI identity report firmware **0.4.6**, so it is classified as the Gate 1 hardware run.

## Result

- 23 harness checks passed; 0 failed; 0 skipped.
- All 16 individual branches passed the 1% limit; maximum absolute error was 0.70669%.
- All three combination masks passed; maximum absolute error was 0.00777%.
- Ten repeated switching cycles passed; maximum absolute error was 0.000285%.
- Heap showed no decline: 129,976 bytes before and 129,984 bytes after.
- Core 1 queue overflow and event-drop counters remained zero.
- Core 1 minimum free stack was 8,160 bytes.
- Final DMM state was open/infinite with all eight masks zero.
- Single-bit apply p95 was 223.70 ms, 2.01% faster than the accepted G0 baseline and within the Gate 1 ±5% criterion.

## Diagnostic gap

The serial evidence file contains harness markers but no captured device `EVT` lines. Internal Core 1 event counters increased with zero drops, so event generation worked but the COM capture/flush path was not demonstrated. Gate 2 adds `SYST:DIAG:SERIAL?`, HIL-008, and batch flushing after Core 1 event drainage.

## Decision

Gate 1 functional, safety, physical-accuracy, memory, and performance criteria are accepted. The serial-observability deficiency is carried as a mandatory Gate 2 regression check rather than blocking numeric-model work.
