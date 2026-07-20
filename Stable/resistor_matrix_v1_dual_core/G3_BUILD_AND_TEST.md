# Gate 3 build and hardware-test sequence

Firmware: **0.6.0**  
Target: Waveshare RP2040 Zero, Arduino-Pico 5.6.1, Arduino CLI 1.5.1

## 1. Production image

From PowerShell in the firmware root:

```powershell
.\setup_arduino_cli_environment.bat
.\arduino_cli_doctor.bat
.\build_firmware.bat
.\build_and_flash_COM17.bat
```

`build_firmware.bat` shows an elapsed-time heartbeat. For every compiler command and diagnostic line:

```powershell
.\build_firmware_verbose.bat
```

Confirm the build manifest identifies firmware 0.6.0 and `test_mode=false`.

## 2. Production regression

From the `RegressionTest` v2.6.0 root:

```powershell
.\run_robot_read_only.bat G3
.\run_robot_safe_output.bat G3
.\run_robot_hil_single_channel.bat G3
```

Production acceptance requires all three profiles to pass, all final masks `0000`, DMM OFF isolation, physical error within the configured limit, coherent HTTP/SCPI snapshots, and no normal-run transport timeout/overflow/generation/fail-safe increments.

## 3. Fault-injection image

Only after the production profiles pass:

```powershell
.\build_and_flash_COM17_gate3_test.bat
```

Confirm the build manifest identifies `test_mode=true`, then run:

```powershell
.\run_robot_gate3_transport_fault.bat G3
```

The fault profile verifies both:

1. a queued command invalidated inside Core 1 is rejected by generation before physical dispatch;
2. a command delayed beyond the Core 0 timeout is later rejected by deadline or generation and never actuates the channel.

Both tests start with all outputs OFF and confirm software masks plus physical DMM isolation.

## 4. Restore production image

After fault injection:

```powershell
.\build_and_flash_COM17.bat
```

Repeat at least the read-only profile to confirm the production image identity and healthy transport counters.

## Gate 3 exit evidence

Archive:

- Arduino build log and `build_manifest.json` for production and test images;
- read-only, safe-output, and HIL Robot result directories;
- Gate 3 fault-profile result directory;
- verified evidence manifests;
- final production reflash identity.
