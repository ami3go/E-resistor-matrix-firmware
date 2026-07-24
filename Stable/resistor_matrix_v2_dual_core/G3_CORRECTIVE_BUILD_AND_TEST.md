# Gate 3 corrective release — firmware v0.6.2

This release corrects two issues exposed by the 2026-07-20 G3 hardware report.

## Corrections

1. USB CDC is started before Core transport and peripheral initialization.
2. The Robot harness asserts DTR before collecting RP2040 USB CDC events.
3. Firmware exposes `SYST:DIAG:USB?` and records `SYST:DIAG:SERIAL?` in the persistent event log.
4. Calibration persistence is explicit through `CAL:STATUS?` and `/state` masks.
5. Missing or invalid calibration files produce startup warnings instead of silently appearing equivalent to a calibrated device.
6. Recovery scripts can extract a saved bundle from a same-board regression archive, back up all eight tables, and restore them with serial-number verification.
7. A BOOTSEL UF2 helper can flash the application when only `RPI-RP2` is visible.

## Required corrective run

```powershell
.\build_firmware.bat
.\flash_firmware_BOOTSEL.bat
.\extract_calibration_from_report.ps1 -ReportZip "C:\path\to\same-board-G2-report.zip"
.\restore_calibration.ps1 -FilePath .\calibration-recovered-YYYYMMDD-HHMMSS.txt
```

The uploaded G3 device serial is `503359277A981F9F`. Do not import the older G1 bundle for `E66178758B3E742A`; recover a same-board bundle or recalibrate. Then run RegressionTest v2.6.2:

```powershell
.\run_robot_read_only.bat G3
.\run_robot_safe_output.bat G3
.\run_robot_hil_single_channel.bat G3
```

Build and flash the test image only after the production profiles pass:

```powershell
.\build_and_flash_COM17_gate3_test.bat
.\run_robot_gate3_transport_fault.bat G3
```

Reflash the production image after fault injection.
