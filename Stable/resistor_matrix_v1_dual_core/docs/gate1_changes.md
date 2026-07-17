# Gate G1 — Low-risk cleanup and observability

Firmware version: **0.4.6**

## Implemented

- Removed direct USB Serial output from Core 1 production paths.
- Added a fixed-size Core 1 event queue and Core 0 event formatter.
- Added compile-time `ERESISTOR_LOG_LEVEL` control.
- Changed `forceAllOff()` and the physical all-off path to return verified status.
- HTTP, SCPI, startup, safe-state, and OTA paths now preserve all-off failures.
- SCPI oversized input now discards the complete remainder of the bad line.
- Enabled Arduino-Pico's separate 8 KiB Core 1 stack.
- Added Core 1 minimum-free-stack, maximum-loop-time, event, and event-drop telemetry.
- Centralized the default address at `192.168.0.55`.
- Replaced channel/bit validation literals with `CHANNEL_COUNT` and `BIT_COUNT` in the Gate G1 scope.
- Removed obsolete split-from-INO and historical source comments.
- Added `tools/check_gate1_source.py`.

## Structured event format

Core 0 prints events received from Core 1 in this form:

```text
EVT ts_us=123456 core=1 seq=42 level=1 code=APPLY_DONE ch=1 mask=0x0005 detail=0 duration_us=2734
```

Core 1 never formats or writes USB Serial text.

## Validation still required on hardware

- Compile with the exact Arduino-Pico and library versions used by the project.
- Run the G1 read-only, safe-output, and `hil_single_channel` profiles.
- Confirm `SCPI-005` returns one overflow error and then accepts `*IDN?`.
- Confirm HIL serial evidence contains `EVT` lines and no event drops.
- Compare apply p95, Core 1 loop maximum, heap stability, and firmware image size against G0.
