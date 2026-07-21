# Optimization Gate 4 — firmware v0.7.0

Gate 4 implements one two-phase eight-channel profile transition owned entirely by Core 1.

## Transition invariant

1. Core 0 validates all eight masks against the immutable safety policy.
2. Core 1 shifts one zero word and pulses every channel latch.
3. Core 1 waits one global `BREAK_BEFORE_MAKE_MS` interval.
4. Core 1 stages and latches all requested channel masks.
5. Core 1 publishes one coherent output snapshot only after the complete profile succeeds.
6. Any failure forces all eight outputs OFF.

Single-channel commands retain their existing per-channel break-before-make behavior.

## Production build

```powershell
.\build_firmware.bat
.\build_and_flash_COM17.bat
```

When the device is in BOOTSEL mode:

```powershell
.\build_firmware.bat
.\flash_firmware_BOOTSEL.bat
```

Restore persistent calibration after a flash layout or filesystem reset using a same-board backup:

```powershell
.\extract_calibration_from_report.ps1 -ReportZip "C:\path\to\same-board-passing-report.zip"
.\restore_calibration.ps1 -FilePath .\calibration-recovered-YYYYMMDD-HHMMSS.txt
```

The restore script blocks a bundle/device serial mismatch by default.

## Gate 4 production regression

Use RegressionTest v2.7.0:

```powershell
.\run_robot_read_only.bat G4
.\run_robot_safe_output.bat G4
.\run_robot_hil_single_channel.bat G4
.\run_robot_gate4_profile.bat G4 <G3-safe-output-results.json>
```

## Gate 4 fault injection

Only after production profiles pass:

```powershell
.\build_and_flash_COM17_gate4_test.bat
```

Then:

```powershell
.\run_robot_gate4_profile_fault.bat G4
```

Reflash the production image afterward.
