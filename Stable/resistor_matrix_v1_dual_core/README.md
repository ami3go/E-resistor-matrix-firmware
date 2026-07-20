# E-Resistor Firmware — Dual-Core + SCPI Calibration + Doxygen

This package is the documented version of the dual-core Arduino RP2040 firmware.

## What is included

- Same pinout as previous firmware.
- Dual-core command architecture: Core 0 communication, Core 1 output control.
- SCPI calibration-table query commands for PC-side nearest-mask calculation.
- Existing firmware-side nearest-mask calculation preserved.
- Doxygen comments added to source files, structs/enums, and functions.
- `Doxyfile` for generating HTML API documentation.
- Human-readable documentation in `docs/`.

## Main files

- `resistor_matrix_v1_dual_core.ino` — Arduino sketch entry file.
- `app.h` — documented shared API and global declarations.
- `core_command.cpp` — dual-core command engine.
- `shift_registers.cpp` — Core 1 physical output driver.
- `scpi_server.cpp` — SCPI command parser and calibration table query support.
- `http_handlers.cpp` — web UI and SCPI web-help page.
- `Doxyfile` — Doxygen configuration.

## Documentation

Open:

```text
docs/html/index.html
```

or read:

```text
docs/firmware_description.md
docs/architecture.md
docs/scpi_reference.md
docs/generated_api_reference.md
docs/build_and_doxygen.md
```

## Generate full Doxygen HTML

```bash
doxygen Doxyfile
```

The full output will be created in:

```text
docs/doxygen/html/index.html
```

## Arduino IDE

Open the `.ino` file from this folder in Arduino IDE and compile for the Waveshare RP2040-Zero / RP2040 target.


## Fast resistance-calculation cache update

This package integrates an optimized `resistance_calculation.cpp` implementation.  Equivalent-resistance calculations now use a cached conductance table (`1/R`) and the firmware-side nearest-mask search uses combination enumeration instead of brute-forcing every 16-bit mask.  The cache is rebuilt at startup after runtime resistor configuration loading and invalidated after any runtime resistor table update.  The firmware-side nearest function is kept for web/manual use, but PC-side drivers should still prefer downloading `CAL:RES?` and calculating nearest masks on the host.

## UI/status update

Latest update adds the requested web-UI changes without changing the hardware pinout:

- Target resistance field on the Control page is widened and uses `10000000` as the 10 MOhm example.
- Resistance now displays 3 digits after the decimal point in Ohm, kOhm, and MOhm ranges.
- The Log page now includes a Boot status table above the event history.
- The `WS2812 heartbeat: GP16` information was removed from the Control page and moved to the Live State / Log status information.
- The Control page now includes an `Identify this board` button. It blinks the WS2812 LED bright blue for 5 seconds using a triple-flash pattern to help identify one board when multiple PCBs are connected.

## Firmware version and browser .bin update

This revision adds firmware identity constants in `app.h`:

- `FIRMWARE_NAME`
- `FIRMWARE_VENDOR`
- `FIRMWARE_VERSION`
- `FIRMWARE_BUILD_DATE`
- `FIRMWARE_BUILD_TIME`

The version is visible in the web top bar, `/state`, `/live`, `/log`, and the new `/firmware` page.
It is also available through SCPI:

```text
*IDN?
SYST:VERS?
FIRM:VERS?
FIRM:BUILD?
SYST:STAT?
```

The new **Firmware** web tab opens `/firmware`, which accepts Arduino-Pico compiled `.bin` firmware files.
Before staging the update, the firmware requests all resistor outputs OFF through the existing safe Core 1 command path.
The upload is staged into LittleFS and then passed to the Arduino-Pico `Update` stream API using the exact staged file size.

Important requirements:

- The first firmware must still be installed by USB/serial.
- Select an Arduino-Pico flash layout with enough LittleFS space for the staged `.bin` file.
- Use `.bin` for web update. Use `.uf2` only for USB BOOTSEL drag-and-drop flashing.
- Do not start a firmware update during calibration or external test operation.


## Calibration file readback for GUI tool

This firmware revision adds explicit readback of device-side calibration/config files so the PC GUI calibration tool can recover tables from a connected board.

HTTP endpoints:

```text
GET /api/calibration/files
GET /api/calibration/download?ch=1
GET /api/calibration/download_all
```

SCPI commands:

```text
CAL:FILES?
CAL:FILE? CH1
CAL:CHAN1:FILE?
CAL:ALL:FILES?
```

`/api/calibration/files` lists `/ch1.csv` ... `/ch8.csv` saved state and file sizes.
`/api/calibration/download?ch=1` returns the active CH1 CSV table.
`/api/calibration/download_all` returns all eight active tables in BEGIN/END blocks.

The browser **Files** tab places LittleFS capacity and its combined file inventory at the top. Its **Download All** button saves the eight-table bundle as a `.txt` file on the PC. **Import from file** restores all eight tables from that same bundle format. Import is accepted only while all channels are OFF; all channel blocks are validated and staged before the runtime calibration is replaced.



## 2026-07-17 optimization Gate 3 — v0.6.0

- Made Core 1 the deterministic and authoritative shift-register/output engine.
- Added fixed-size command, result, and numeric event queues with no dynamic allocation.
- Added command sequence, absolute deadline, and safety-generation validation before physical dispatch.
- Added semaphore-backed result waiting and Core 1 startup synchronization.
- Added immutable double-buffered calibration/safety policy handoff while all outputs are OFF.
- Added atomic output snapshots containing all masks, apply counters, generation, sequence, and safety flags.
- Added `SYST:CORE:TRANSPORT?` and `SYST:CORE:SNAPSHOT?` diagnostics.
- Added Core 1 direct all-OFF handling for Core 0 timeout and heartbeat loss.
- Added production-disabled fault-injection hooks plus dedicated Gate 3 test-image build launchers.
- Added Robot-only Gate 3 transport, coherence, stress, HIL, queued-invalidation, and timeout no-ghost-actuation coverage.
- See `docs/gate3_changes.md`, `G3_BUILD_AND_TEST.md`, and `GATE3_RELEASE_STATUS.md`.

## 2026-07-17 optimization Gate 2 — v0.5.0

- Replaced the 8 × 16 text-heavy runtime resistor table with a direct numeric `float` table indexed by channel and bit.
- Stores the common bit-to-MOSFET mapping once and generates display names at text boundaries.
- Replaced eight duplicate compile-time default tables with one shared 16-value default table.
- Preserved the existing CSV, header-initializer, SCPI calibration, browser backup, and import/export formats.
- Added strict finite, positive, bounded calibration parsing and rejection of trailing resistance text.
- Removed per-candidate `logf()` from nearest-mask search and compares relative resistance error using conductance cross-products.
- Added target-search deadline, cancellation checkpoint, candidate count, elapsed-time, timeout, and cancellation telemetry.
- Added the read-only SCPI command `CH<n>:TARGET:CALC? <ohm>`; it returns a mask and calculation diagnostics without changing outputs.
- Added `SYST:DIAG:SERIAL?` to emit a deterministic structured USB diagnostic event for regression testing.
- Added Gate 2 host oracle vectors from the accepted Gate 1 calibration snapshot and automated source acceptance checks.
- Estimated static RAM reduction from the runtime calibration model is 2,688 bytes; exact linked RAM and firmware size remain build acceptance measurements.

## 2026-07-16 optimization Gate 1 — v0.4.6

- Removed direct `Serial.print`, `Serial.println`, and `Serial.flush` calls from Core 1 production paths.
- Added a compact fixed-size Core 1 event queue; Core 0 formats diagnostic lines as structured `EVT` records.
- Changed all-OFF wrappers and physical handling to return verified success/failure and preserved the failure reason in HTTP, SCPI, startup, safe-state, and OTA paths.
- Fixed oversized SCPI-line recovery so the remainder of an invalid line is discarded until newline and cannot execute as a second command.
- Enabled a separate Core 1 stack and added Core 1 loop, stack, event, and event-drop telemetry.
- Centralized the default device address as `192.168.0.55`.
- Removed obsolete split-from-monolithic-sketch comments and moved algorithm history to `docs/performance_history.md`.
- Added `tools/check_gate1_source.py` for automated Gate 1 source acceptance checks.

## 2026-07-15 calibration bundle backup update — v0.4.4

- The Files tab now shows LittleFS storage and file inventory before calibration backup controls.
- **Download All** saves one named text file containing CH1-CH8 active calibration tables.
- **Import from file** validates, persists, and activates all eight tables from one exported file.
- Restore uses temporary files and backups so malformed or partially staged imports do not replace the active runtime tables.
- Calibration import is rejected while any channel mask is active.

## 2026-07-15 web GUI consolidation update

- The Control tab no longer renders eight separate resistor tables.
- The Calibration tab now starts with one scrollable 16-row table containing CH1 through CH8 resistor values.
- Safety limits are independently configurable and enforced for every channel: minimum resistance, maximum resistance, and maximum active bits.
- Existing legacy global `safety.csv` files remain supported; their values are applied to all eight channels on load. New saves use the per-channel version-2 format.
- The Files tab now combines expected channel calibration files and all LittleFS root files into one inventory without duplicate calibration rows.
- The Firmware tab no longer repeats the LittleFS file inventory and links to the Files tab instead.
- The Log tab can export the current boot information and in-memory event history as a `.txt` file.

## 2026-06-24 UI maintenance update

This package includes additional web UI maintenance features:

- Firmware version/build and board serial are shown together in the top status bar and in `/state` as `firmware_serial`.
- The control page has a full-card clickable **Device identification** control that blinks the WS2812 LED on GP16 bright blue for 5 seconds.
- Manual channel-control mask/apply forms were tightened so the mask field and Apply button stay on one row.
- The Calibration page can now delete per-channel metadata files in addition to saving them.
- The Ethernet page can delete saved network settings from LittleFS and restore default RAM values.
- The Files page now shows a Delete button for every root-level LittleFS file.

The delete buttons operate on LittleFS configuration/data files only; they do not change the fixed pinout or shift-register mapping.

## UI compact layout update

- Web UI display name changed from the RP2040 matrix name to **E-Resistor**.
- Manual channel control table label changed from **Calculated output resistance** to **Resistance**.
- Manual channel control table spacing was reduced to avoid horizontal scrolling on normal desktop displays.
- Bit indicators, target field, mask field, and Apply buttons were compacted while preserving all existing routes and control behavior.

## Web firmware update LittleFS note

If the Firmware tab reports `LittleFS staging short write: 0/1436` or a similar short-write message, the uploaded `.bin` file is usually not the problem. The number after the slash is only one HTTP upload chunk. The usual cause is that the board was compiled/flashed with a Flash Size option that has no LittleFS partition, or the LittleFS partition is too small/full.

For web `.bin` update, select an Arduino-Pico **Tools > Flash Size** option that includes enough filesystem space, then flash once by USB. Example: a layout with about 1 MB FS is enough for typical ~300 kB firmware binaries.

## Live/backup/SCPI/safety UI cleanup

- The Live State tab now contains only the live `/state` plain-text block and the channel-mask table.
- The Backup tab factory-reset actions are arranged in a spaced grid instead of stacked tightly.
- The SCPI page now labels the connection field as **Address** and shows `IP:5025`.
- The Safety page now includes both minimum and maximum allowed calculated resistance limits in ohms.

## SCPI web reference table (introduced in v0.4.3)

- Replaced the SCPI tab's plain command list with a two-column **Command / Description** table.
- Kept descriptions short and grouped equivalent command aliases in the same row.
- Documented channel placeholders as `CH<n>` for channels 1 through 8.

## Arduino CLI reproducible Windows build

Gate 1 packages include local Arduino CLI setup, build, and UF2 upload scripts. From the sketch root run:

```bat
setup_arduino_cli_environment.bat
build_firmware.bat
flash_firmware_COM17.bat
```

The normal build prints a periodic elapsed-time heartbeat; `build_firmware_verbose.bat` streams every compiler command live.

The configuration matches the documented Waveshare RP2040 Zero IDE settings: 2 MB flash split as 1 MB sketch plus 1 MB LittleFS, 200 MHz CPU, IPv4-only 32 KB lwIP, `-Os`, Pico SDK USB, no OS, exceptions, RTTI, profiling, stack protector, or debug output. See `tools/arduino_cli/README.md`.
