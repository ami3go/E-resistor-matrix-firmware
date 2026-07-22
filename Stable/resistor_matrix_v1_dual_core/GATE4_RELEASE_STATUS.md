# Gate 4 release status

- Firmware: **0.7.2**
- Gate: **G4**
- Accepted baseline: **Gate 3 firmware 0.6.2**
- Board used for accepted Gate 3 evidence: `503359277A981F9F`
- Status: **implementation complete; target build and HIL acceptance pending**

## Implemented

- Preserves the accepted v0.6.2 startup-handshake corrections.
- Validates all eight profile masks before physical switching.
- Clears all eight outputs with one shared zero shift and eight latch pulses.
- Uses one global break-before-make delay per profile.
- Publishes only the final coherent eight-channel snapshot.
- Forces all outputs OFF after clear or make failure.
- Adds bounded profile diagnostics and structured events.
- Adds a compile-time failure-after-clear test hook.

## Offline validation

- Gate 4 structural checks: 25/25 passed.
- Profile model oracle: 5/5 passed.
- C/C++ lexical checks: 26/26 passed.
- Host syntax-only checks: 6/6 passed.

These checks do not replace Arduino-Pico target compilation or connected HIL testing.
