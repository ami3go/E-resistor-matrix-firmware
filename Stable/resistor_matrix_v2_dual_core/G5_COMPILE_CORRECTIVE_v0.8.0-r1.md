# Gate 5 firmware compile corrective — v0.8.0-r1

## Reported failure

Arduino IDE stopped in `http_pages.cpp` with:

```text
error: 'CALIBRATION_BUNDLE_MAX_BYTES' was not declared in this scope
```

The constant was defined as a file-local `static constexpr` in
`http_maintenance.cpp`, but the Files page also references it from
`http_pages.cpp`. Separate C++ translation units cannot see file-local symbols.

## Correction

- Moved the limit to `http_api.h` as an `inline constexpr` shared definition.
- Removed the private duplicate from `http_maintenance.cpp`.
- Added an offline source check preventing the limit from becoming private again.
- Firmware protocol identity remains `0.8.0`; the distributable package revision is `r1`.

## Toolchain note

The submitted log used Arduino-Pico core `6.0.0`. Gate 5 acceptance remains pinned
to core `5.6.1`. The source correction is standard C++ and is intended to compile
with either version, but formal regression evidence must use the pinned core.

## Validation performed

Offline structural, lexical, focused host-syntax, and service-oracle checks were
rerun. Arduino-Pico target compilation still requires Arduino IDE or Arduino CLI
with the RP2040 core installed.
