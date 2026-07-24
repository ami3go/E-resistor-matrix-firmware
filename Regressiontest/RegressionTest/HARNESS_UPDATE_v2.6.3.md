# RegressionTest v2.6.3 — Gate 3 startup corrective compatibility

## Purpose

Firmware v0.6.2 corrects the Core 0/Core 1 boot-handshake race found on hardware in v0.6.1. This harness release preserves the v2.6.2 Robot-only test behavior and recognizes firmware v0.6.2 as Gate G3.

## Changes

- Package and Robot library version advanced to 2.6.3.
- Firmware 0.6.2 maps to Gate G3.
- Offline version-mapping test covers 0.6.0, 0.6.1, and 0.6.2.
- Documentation now names v0.6.2 as the required corrective G3 production firmware.
- Internal ZIP root remains exactly `RegressionTest/`.

## Required Gate 3 closure runs

```powershell
.\run_robot_read_only.bat G3
.\run_robot_safe_output.bat G3
.\run_robot_hil_single_channel.bat G3
.\run_robot_gate3_transport_fault.bat G3
```

The fault-injection image must be replaced with the v0.6.2 production image after the fault profile completes.
