# Gate 3 dual-core architecture

Firmware 0.6.2 uses an explicit ownership and message-transport boundary. Core 0 never directly toggles resistor shift-register GPIO. Core 1 never performs networking, filesystem, UI, SCPI/HTTP parsing, dynamic text construction, or USB serial formatting.

```text
Core 0                                        Core 1
────────────────────────────────────          ────────────────────────────────────
HTTP / SCPI / Ethernet / LittleFS             Shift-register GPIO ownership
Mutable configuration and target math   -->   Envelope + policy validation
Fixed command submission queue                Break-before-make physical apply
Bounded result wait                     <--   Fixed numeric result queue
Human-readable event formatting         <--   Fixed numeric event queue
Atomic snapshot reader                  <--   Atomic authoritative snapshot writer
Core 0 heartbeat                         -->   Direct all-OFF fail-safe monitor
```

Every output command is fixed-size and carries a sequence, deadline, and safety generation. Core 1 validates all three before entering the physical dispatcher. Calibration and safety values cross the boundary only as immutable policy snapshots installed while all outputs are OFF.


```text
Core 0: Communication and UI
┌──────────────────────────────────────────────┐
│ Ethernet/W5500 + lwIP                         │
│ HTTP WebServer                                │
│ SCPI TCP Server                               │
│ LittleFS configuration/profile handling       │
│ HTML/status generation                        │
│ Command validation                            │
└───────────────────────┬──────────────────────┘
                        │ bounded request/response queue
                        ▼
Core 1: Hardware Safety Engine
┌──────────────────────────────────────────────┐
│ Shift-register DATA/CLOCK/SRCLR               │
│ Channel latch GPIOs                           │
│ Break-before-make switching                   │
│ All-OFF / safe-state execution                │
│ Channel mask shadow state                     │
│ Emergency OFF flag handling                   │
└──────────────────────────────────────────────┘
```

## Module map

| File | Responsibility |
|---|---|
| `app.h` | Shared declarations, constants, globals, and public APIs |
| `app_globals.cpp` | Global objects and runtime state definitions |
| `board_config.h` | Default resistor branch table and fixed hardware mapping |
| `core_command.cpp` | Core 0 command submission, bounded wait, timeout invalidation, and safe wrappers |
| `core_transport_types.h` | Fixed numeric cross-core protocol, snapshots, policy, and diagnostics types |
| `core_transport.cpp` | Bounded queues, locks, semaphores, generation control, policy slots, and snapshots |
| `core1_output_engine.cpp` | Core 1 envelope validation and deterministic command dispatch |
| `core1_runtime.cpp` | Core 1 startup, physical all-OFF initialization, and loop timing |
| `shift_registers.cpp` | Core 1-only physical shift-register and latch operations |
| `setup_loop.cpp` | Core 0 and Core 1 Arduino entry points |
| `scpi_server.cpp` | SCPI TCP parser and command execution |
| `http_handlers.cpp` | Web UI and HTTP route handlers |
| `runtime_resistor_config.cpp` | Runtime calibration table parsing/storage |
| `resistance_calculation.cpp` | Resistance math, mask safety, and profile helpers |
| `ethernet_startup.cpp` | W5500/lwIP static IPv4 startup |
| `status_led.cpp` | WS2812 heartbeat/fault indicator |
| `w5500_registers.cpp` | W5500 low-level diagnostic register access |
| `utility.cpp` | Status, logging, runtime monitor, config helpers |

## Threading rule

Only Core 1 may physically modify resistor outputs. Core 0 may request output changes but must not directly toggle shift-register or latch GPIOs.

## Gate 3 policy boundary

Core 0 owns mutable numeric calibration and safety configuration. Core 1 receives only immutable fixed-size `CoreSafetySnapshot` copies. Policy staging and installation require a coherent all-OFF state and advance the safety generation.

## Gate 2 numeric model boundary

Core 0 owns the mutable numeric calibration table and target calculation. Core 1 continues to receive physical mask commands and does not run target searches. Calibration updates invalidate the conductance cache. They must be performed while outputs are OFF.
