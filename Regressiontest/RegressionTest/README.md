# E-Resistor RegressionTest v2.7.0

This package targets firmware **0.7.0 / Gate G4** and exposes Robot Framework workflows only. The internal ZIP root remains `RegressionTest/`.

## Prerequisite: close corrected Gate 3

Firmware 0.6.1 and RegressionTest 2.6.2 must first produce passing:

```powershell
.\run_robot_read_only.bat G3
.\run_robot_safe_output.bat G3
.\run_robot_hil_single_channel.bat G3
.\run_robot_gate3_transport_fault.bat G3
```

The corrected G3 safe-output run contains `G3-004.profile_internal_p95_ms`, which is the required Gate 4 performance baseline.

## Gate 4 production sequence

```powershell
.\setup_robot_environment.bat
.\run_robot_read_only.bat G4
.\run_robot_safe_output.bat G4
.\run_robot_hil_single_channel.bat G4
.\run_robot_gate4_profile.bat G4 "C:\path\to\G3-safe_output-...\e_resistor_evidence\safe_output\results.json"
```

The profile runner executes 1,000 all-zero eight-channel profile transitions and verifies one global break-before-make operation per transition.

## Gate 4 fault injection

Flash firmware built with `build_firmware_gate4_test.bat`, then run:

```powershell
.\run_robot_gate4_profile_fault.bat G4
```

Reflash production firmware afterward.

## Bench configuration

Copy:

```text
robot_framework\variables\bench_config.example.bat
```

to:

```text
robot_framework\variables\bench_config.local.bat
```

For HIL and fault profiles, confirm the DMM is connected only to the selected E-Resistor channel and set:

```bat
set "ERESISTOR_ALLOW_ACTIVE_OUTPUT_TESTS=true"
set "ERESISTOR_FIXTURE_CONFIRMATION=E_RESISTOR_SINGLE_CHANNEL_DMM"
```

Every run finalizes a SHA-256 evidence manifest.
