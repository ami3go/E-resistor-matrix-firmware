# Optimization Gate 4 changes — firmware v0.7.1

Gate 4 is based directly on the accepted Gate 3 production baseline v0.6.2. It preserves the corrected Core 0/Core 1 startup handshake, USB CDC behavior, cross-core memory barriers, calibration persistence diagnostics, and fault-safe transport.

## Two-phase eight-channel profile transition

`ROUT:ALL:MASK` and other full-profile operations now use one global break-before-make sequence:

1. Validate all eight requested masks against the installed safety policy.
2. Shift one zero word and pulse every channel latch, clearing all eight physical outputs.
3. Wait one global `BREAK_BEFORE_MAKE_MS` interval.
4. Shift and latch each requested channel mask.
5. Publish one coherent eight-channel snapshot only after the complete make phase succeeds.

The previous Gate 3 implementation applied eight independent channel transitions and therefore waited eight break-before-make intervals per profile.

## Failure handling

A clear-phase or make-phase failure immediately calls the Core 1 physical all-OFF path. Intermediate profile states are not published to Core 0. The command completes with an error and the final coherent snapshot reports the physical safe state.

## Diagnostics

- `SYST:CORE:PROFILE?`
- profile counters in `SYST:CORE:TRANSPORT?`
- profile counters in HTTP `/state`
- `PROFILE_BEGIN`, `PROFILE_BREAK_BEFORE_MAKE`, `PROFILE_DONE`, and `PROFILE_FAILED` structured events

## Test image

The compile-time test image adds:

```text
SYST:TEST:PROFILE:FAIL:NEXT
```

The command is accepted only while all eight outputs are OFF. It forces the next full-profile operation to fail after the global clear phase. The expected result is an error response, all software masks zero, and physical DMM isolation.
