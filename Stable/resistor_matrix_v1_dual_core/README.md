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

- `rp2040_w5500_resistor_matrix_v1_dual_core_cal_scpi_webhelp_doxygen.ino` — Arduino sketch entry file.
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
