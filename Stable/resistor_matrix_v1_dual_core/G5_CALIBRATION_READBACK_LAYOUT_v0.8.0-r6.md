# G5 calibration readback layout corrective v0.8.0-r6

## Purpose

Move the **Calibration file readback** workflow from the Files tab to the Calibration tab so calibration-table viewing, backup, and restore are grouped in one operator workflow.

## Firmware changes

- Added a streamed Calibration file readback card to `handleSettings()` in `http_calibration_page.cpp`.
- Removed the Calibration file readback card from `handleFilesPage()` in `http_pages.cpp`.
- Kept the Files tab focused on LittleFS storage and file inventory.
- Changed the all-channel import success redirect from `/files?cal_import=ok` to `/settings?cal_import=ok`.
- Updated static asset cache revision to `calreadback-r6`.

## Compatibility

- Calibration import/export routes are unchanged.
- Calibration bundle text format is unchanged.
- SCPI behavior is unchanged.
- Resistor output control and Core 1 hardware engine behavior are unchanged.

## Offline validation

- C/C++ lexical check: 48/48 PASS.
- Gate 5 structural source check: 38/38 PASS.
- Gate 5 service oracle: 10/10 PASS.

## Required target validation

Compile and flash on the RP2040 target, then verify:

1. Calibration tab shows the **Calibration file readback** card.
2. Files tab no longer contains the readback/import card.
3. Download All still returns the all-channel calibration bundle.
4. Import from file still restores all eight tables when outputs are OFF.
5. Successful import returns to the Calibration tab and shows the success message.
