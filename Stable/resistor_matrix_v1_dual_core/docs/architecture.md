# Firmware Architecture

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
| `core_command.cpp` | Dual-core command queue and Core 1 dispatch |
| `shift_registers.cpp` | Physical shift-register and latch operations |
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
