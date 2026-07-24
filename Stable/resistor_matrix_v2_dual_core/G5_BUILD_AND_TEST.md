# Gate 5 build and test procedure

## Release identities

- Firmware: **v0.8.0**
- Regression package: **v2.8.1 or later**
- Accepted predecessor: Gate 4 firmware **v0.7.2**
- Target board: Waveshare RP2040 Zero
- Arduino-Pico core: **5.6.1**

## Arduino IDE settings

Open `resistor_matrix_v1_dual_core.ino` from this folder and select:

```text
Board:                 Waveshare RP2040 Zero
CPU Speed:             200 MHz
Flash Size:            2MB (Sketch 1MB / FS 1MB)
Optimize:              Small (-Os)
USB Stack:             Pico SDK
IP/Bluetooth Stack:    IPv4 Only - 32K
Debug Port:            Disabled
Debug Level:           None
C++ Exceptions:        Disabled
RTTI:                  Disabled
Stack Protector:       Disabled
Operating System:      None
Upload Method:         Default / UF2
```

Required library:

```text
Adafruit NeoPixel 1.15.5
```

Use a fresh extracted folder so Arduino IDE cannot reuse objects from v0.7.2.

## Boot checks

The serial log must reach:

```text
Core 1 hardware engine ready; all outputs OFF; startup stage=7
HTTP server started on port 80 (API v1)
SCPI server started on TCP port 5025
```

Then verify:

```text
*IDN?
OpenBench,E-Resistor,<serial>,0.8.0
```

```text
GET /api/v1/health
GET /api/v1/state
GET /api/v1/diagnostics
```

The production build must report `test_mode=0`, all channel masks `0000`, and calibration saved/loaded masks `255`.

## Gate 5 Robot execution

Extract RegressionTest v2.8.1 so its internal folder remains exactly `RegressionTest/`, then run:

```powershell
.\setup_robot_environment.bat
.\run_robot_read_only.bat G5
.\run_robot_safe_output.bat G5
.\run_robot_gate5_service.bat G5
.\run_robot_hil_single_channel.bat G5
```

The HIL run requires the established single-channel DMM fixture and its existing confirmation interlock.

## Gate 5 closure limits

- Calibration page temporary heap reduction: **at least 50%** from the embedded Gate 4 baseline.
- `/state` p95: no more than **5% above 98.349025 ms** on the accepted bench baseline.
- Browser page requests: **at least 1,000**, with persistent free-heap decline no greater than the configured limit.
- HTTP mutation GET probes: all return **405** with `Allow: POST`; no output command is submitted.
- API v1 and legacy calibration aliases: coherent.
- SCPI aliases: compatible; HELP and browser SCPI table generated from the same registry.
- Full physical HIL: passes while HTTP polling remains active and SCPI state polling is counted throughout output operations.
- Final masks: `0000`; final DMM isolation passes.
