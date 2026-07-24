# E-Resistor Robot Framework RegressionTest v2.8.1

This Robot-only package validates **Gate 5 firmware v0.8.0**. The internal ZIP root is always exactly `RegressionTest/`.

## Gate 5 acceptance profiles

Run in this order:

```powershell
.\setup_robot_environment.bat
.\run_robot_read_only.bat G5
.\run_robot_safe_output.bat G5
.\run_robot_gate5_service.bat G5
.\run_robot_hil_single_channel.bat G5
```

`gate5_service` validates:

- flash-backed cacheable CSS and JavaScript;
- browser page-content smoke tests;
- `/api/v1/` JSON schemas and compatibility aliases;
- POST-only mutation paths and safe HTTP 405 responses to GET;
- SCPI command aliases and shared HELP/browser registry;
- at least 50% calibration-page temporary-heap reduction;
- 1,000 browser page requests without persistent heap decline;
- `/state` p95 no more than 5% above the accepted Gate 4 baseline.

The Gate 5 HIL additions keep HTTP state polling active through the physical bit walk, combination measurements, and 50-cycle repeatability test. SCPI state polls on the active control connection are counted throughout the same interval. This avoids a second persistent SCPI socket because the firmware deliberately supports one SCPI client at a time.

## Accepted baseline

The accepted Gate 4 read-only evidence is bundled at:

```text
baselines\G4_v0.7.2_board_503359277A981F9F\results.json
```

Its `/state` p95 is `98.349025 ms`. `run_robot_gate5_service.bat` uses this file automatically.

The accepted Gate 3 profile baseline remains bundled for historical Gate 4 reruns:

```text
baselines\G3_v0.6.2_board_503359277A981F9F\results.json
```

## Bench configuration

Copy:

```text
robot_framework\variables\bench_config.example.bat
```

to:

```text
robot_framework\variables\bench_config.local.bat
```

For HIL, the DMM must be connected only to the selected E-Resistor channel. Enable the fixture intentionally:

```bat
set "ERESISTOR_ALLOW_ACTIVE_OUTPUT_TESTS=true"
set "ERESISTOR_FIXTURE_CONFIRMATION=E_RESISTOR_SINGLE_CHANNEL_DMM"
```

All other channels must remain OFF. Every run writes Robot output, protocol/DMM/serial evidence as applicable, coverage traceability, package/Python/Robot/device versions, and a verified SHA-256 evidence manifest.

## Historical diagnostic profiles

The package retains the accepted Gate 3 and Gate 4 diagnostic runners for reproducibility:

- `run_robot_gate3_transport_fault.bat`
- `run_robot_gate4_profile.bat`
- `run_robot_gate4_profile_fault.bat`

They are not part of the Gate 5 normal acceptance sequence.
