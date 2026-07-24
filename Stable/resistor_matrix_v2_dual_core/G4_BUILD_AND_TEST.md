# Gate 4 build and test

## Arduino IDE production image

Open `resistor_matrix_v1_dual_core.ino` and use:

- Board: Waveshare RP2040 Zero
- Arduino-Pico core: 5.6.1
- CPU: 200 MHz
- Flash: 2 MB, Sketch 1 MB / FS 1 MB
- Optimize: Small (`-Os`)
- USB stack: Pico SDK
- IP stack: IPv4 only, 32K
- Debug port and level: Disabled / None
- Exceptions, RTTI, stack protector: Disabled
- Operating system: None
- Upload: Default UF2

After flashing, verify:

```text
*IDN?                 -> OpenBench,E-Resistor,<serial>,0.7.2
SYST:CORE:PROFILE?    -> profile diagnostics
/state                -> firmware_version=0.7.2 and test_mode=0
```

## Arduino CLI production image

```powershell
.\build_firmware.bat
.\build_and_flash_COM17.bat
```

## Gate 4 fault-injection image

Arduino CLI:

```powershell
.\build_and_flash_COM17_gate4_test.bat
```

Arduino IDE users should use the separate `E-Resistor_Firmware_Gate4_v0.7.2_ArduinoIDE_TEST.zip` package. Confirm:

```text
SYST:TEST:MODE? -> 1
```

## Robot Framework execution

Using RegressionTest v2.7.1:

```powershell
.\run_robot_read_only.bat G4
.\run_robot_safe_output.bat G4
.\run_robot_hil_single_channel.bat G4
.\run_robot_gate4_profile.bat G4
```

Then flash the Gate 4 test image and run:

```powershell
.\run_robot_gate4_profile_fault.bat G4
```

Restore production firmware and finish with:

```powershell
.\run_robot_read_only.bat G4
```
