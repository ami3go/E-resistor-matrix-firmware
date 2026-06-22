# RP2040 W5500 Resistor Matrix Firmware — Dual-Core + SCPI Calibration + Doxygen

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
