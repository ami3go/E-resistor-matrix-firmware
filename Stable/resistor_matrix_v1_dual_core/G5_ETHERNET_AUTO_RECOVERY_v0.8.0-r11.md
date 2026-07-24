# Gate 5 Ethernet auto-recovery corrective — v0.8.0-r11

## Problem

Earlier Gate 5 startup treated an unplugged Ethernet cable as a fatal W5500
hardware failure. `w5500SoftwareResetAndProbe()` rejected `PHYCFGR.LNK=0`, called
`safeState()`, and returned from `setup()` before lwIP, HTTP, and SCPI were
started. Connecting the cable later could not recover communications without a
board reset.

## Corrective architecture

- W5500 presence and cable link are now separate conditions.
- `VERSIONR == 0x04` proves that the W5500 hardware is present.
- `PHYCFGR.LNK == 0` is a recoverable `wait_link` condition, not a fatal safe
  state.
- All eight outputs are explicitly forced OFF before Ethernet initialization.
- Static lwIP initialization and HTTP/SCPI listener creation occur even when the
  cable is absent.
- A Core 0 recovery service polls physical link state every 1000 ms.
- Connecting the cable transitions automatically from `wait_link` to `online`.
- Disconnecting the cable closes a stale SCPI client but retains the HTTP and
  SCPI listeners for reconnection.
- If W5500 hardware is not detected at boot, initialization is retried every
  5000 ms.
- Direct diagnostic SPI reads are bracketed by
  `ethernet_arch_lwip_begin()/ethernet_arch_lwip_end()` after lwIP starts, so
  they do not collide with the Ethernet polling callback.

## Output safety behavior

- During boot without a cable, all channels remain OFF.
- Cable loss after normal operation does **not** change the programmed channel
  masks. This preserves the prior runtime output behavior while communications
  recover automatically.
- Core 1 faults and other fatal firmware safety faults still use `safeState()`
  and force all outputs OFF.

## New diagnostics

The legacy `/state`, `/api/v1/state`, `/api/v1/diagnostics`, Live State page,
and exported event log now report:

- Ethernet interface started
- HTTP/SCPI services started
- Physical link up/down
- Recovery state (`uninitialized`, `hardware_retry`, `wait_link`, `online`)
- Initialization/recovery attempts
- Successful recoveries
- Link-down transition count
- Time since the last link transition

## Required HIL validation

1. Boot with the cable connected; verify HTTP and SCPI normally.
2. Boot with the cable disconnected; verify outputs are OFF and the serial log
   reports `wait_link` without `SAFE STATE`.
3. Connect the cable after at least 30 seconds; verify HTTP `/ping`, `/state`,
   `/api/v1/health`, and SCPI `*IDN?` work without resetting the board.
4. Disconnect and reconnect the cable five times; verify recovery on every
   cycle and no Core 1 command timeout, queue overflow, or output-mask change.
5. Keep the cable disconnected for ten minutes; verify Core 0 and Core 1
   heartbeats continue and no restart occurs.
6. Run the complete Gate 5 Robot Framework suite after recovery.

## Offline validation completed

- C/C++ lexical source check: 48/48 PASS
- Gate 5 structural checks: 48/48 PASS
- Gate 3 focused host syntax checks: PASS
- Gate 4 focused host syntax checks: PASS

Arduino-Pico target compilation and connected hardware validation remain
required.
