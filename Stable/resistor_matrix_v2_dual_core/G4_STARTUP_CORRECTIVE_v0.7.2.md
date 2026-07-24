# Gate 4 startup corrective release v0.7.2

The v0.7.1 two-phase profile algorithm is retained. This release hardens the
Core 0/Core 1 startup handshake and adds deterministic startup diagnostics.

## Changes

- Replaced the consumable Core-1-ready semaphore with a persistent one-way token.
- Core 0 polls both the token and the coherent output snapshot, so an early
  Core 1 notification cannot be consumed or lost.
- Added startup stages 0..8 and explicit GPIO/all-OFF failure stages 0x81/0x82.
- Increased the bounded Core 0 startup wait from 5 s to 10 s.
- Added `core1_startup_stage` and `core1_ready_token` to HTTP `/state`.
- Added `startup_stage` and `ready_token` to `SYST:CORE:TRANSPORT?`.
- Added detailed serial diagnostics before entering the red safe-state indication.

Expected successful serial line:

```text
Core 1 hardware engine ready; all outputs OFF; startup stage=7
```

The stage advances to `8` once `loop1()` begins.
