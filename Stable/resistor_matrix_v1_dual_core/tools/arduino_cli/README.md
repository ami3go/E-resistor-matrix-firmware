# Arduino CLI build and upload scripts

These Windows scripts reproduce the Arduino IDE settings used for the E-Resistor firmware.

## Configured board

- Board: Waveshare RP2040 Zero
- FQBN: `rp2040:rp2040:waveshare_rp2040_zero`
- Port: `COM17`
- Debug level: None
- Debug port: Disabled
- C++ exceptions: Disabled
- Flash: 2 MB total, 1 MB sketch and 1 MB LittleFS
- CPU: 200 MHz
- IP/Bluetooth stack: IPv4 Only - 32K
- Optimization: Small `-Os`
- Operating system: None
- Profiling: Disabled
- RTTI: Disabled
- Stack protector: Disabled
- Upload method: Default UF2
- USB stack: Pico SDK

## Quick start

Run from the firmware root folder:

```bat
setup_arduino_cli_environment.bat
arduino_cli_doctor.bat
build_firmware.bat
flash_firmware_COM17.bat
```

Or build and flash in one operation:

```bat
build_and_flash_COM17.bat
```


## Live progress and verbose builds

The standard build shows four major phases and prints an elapsed-time heartbeat every three seconds while Arduino CLI is quiet:

```bat
build_firmware.bat
```

For complete live Arduino CLI and compiler command output, use:

```bat
build_firmware_verbose.bat
```

The same mode can be selected directly:

```bat
build_firmware.bat -VerboseBuild
```

Change the heartbeat interval when needed:

```bat
build_firmware.bat -HeartbeatSeconds 5
```

Both modes stream output to the console and write it immediately to the timestamped build log. The verbose log is much larger because Arduino CLI prints every external compiler command when `--verbose` is enabled.

## Installed tool versions

The setup is pinned for reproducibility:

- Build script 1.1.1
- Arduino CLI 1.5.1
- Arduino-Pico core 5.6.1
- Adafruit NeoPixel 1.15.5

All packages are installed inside the firmware folder under `.tools/` and `.arduino-cli/`. The scripts do not change the Arduino IDE installation.

## Change the COM port

Either pass a port to PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\arduino_cli\arduino_cli.ps1 -Action BuildFlash -Port COM18
```

or set an environment variable before running a generic action:

```bat
set ERESISTOR_COM_PORT=COM18
build_and_flash_COM17.bat
```

The dedicated `build_and_flash_COM17.bat` and `flash_firmware_COM17.bat` intentionally force COM17. Copy and rename them when a permanent different port is needed.

## Output

Build outputs are stored in:

```text
.build\arduino-cli\waveshare_rp2040_zero\
.build\arduino-cli\dist\
.build\arduino-cli\logs\
```

The `dist` folder contains UF2, BIN, ELF and MAP files when generated, plus `build_manifest.json` with SHA-256 hashes and all board options.

## Upload recovery

The default UF2 upload method normally resets the RP2040 through the selected COM port. When automatic reset fails:

1. Disconnect USB.
2. Hold BOOTSEL.
3. Reconnect USB.
4. Release BOOTSEL.
5. Run `flash_firmware_COM17.bat` again.

## Gate 3 production and fault-injection builds

Normal production build and flash:

```bat
build_firmware.bat
build_and_flash_COM17.bat
```

Gate 4 fault-injection test image with `ERESISTOR_TEST_MODE=1`:

```bat
build_firmware_gate3_test.bat
build_and_flash_COM17_gate3_test.bat
```

The test image exposes bounded, all-OFF-interlocked Core 1 fault hooks. It must be used only for the dedicated Robot fault profile. Reflash the production image immediately afterward. `build_manifest.json` records whether `test_mode` was enabled.
