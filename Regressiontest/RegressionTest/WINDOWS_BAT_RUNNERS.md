# Windows BAT Test Runners

The archive includes Windows launchers in the fixed `RegressionTest` root.

## First-time setup

Run:

```bat
setup_robot_environment.bat
```

This creates `.venv`, installs the Robot/HIL dependencies, validates the Robot suites, and creates:

```text
robot_framework\variables\bench_config.local.bat
```

Edit that local file with the IP address, COM port, DMM VISA resource, source directory, and gate-specific settings. The local file is not part of release archives, so it can remain unchanged when a newer ZIP is extracted over the same `RegressionTest` folder.

## Launchers

| BAT file | Purpose | Active resistance output |
|---|---|---:|
| `run_robot_read_only.bat` | HTTP, SCPI, calibration, timing, stress and file-download checks | No |
| `run_robot_safe_output.bat` | Read-only checks plus zero-mask and repeated `ALL:OFF` tests | No nonzero mask |
| `run_robot_hil_single_channel.bat` | SCPI + USB serial + DMM physical regression | Yes, selected channel |
| `run_robot_source_check.bat` | Offline source metrics and gate checks | No hardware output |
| `run_robot_all_safe.bat` | Read-only, safe-output and configured source-check profiles | No nonzero mask |
| `run_robot_custom.bat` | Pass custom arguments directly to `run_robot.py` | Depends on arguments |
| `validate_robot_suites.bat` | Robot dry-run/syntax validation | No |
| `generate_robot_keyword_docs.bat` | Regenerate Robot library HTML documentation | No |

Pass a gate as the first argument to the standard runners:

```bat
run_robot_read_only.bat G3
run_robot_safe_output.bat G3
run_robot_hil_single_channel.bat G3
run_robot_source_check.bat G3
run_robot_all_safe.bat G3
```

## HIL safety authorization

The HIL launcher refuses to run until both settings are deliberately enabled in `bench_config.local.bat`:

```bat
set "ERESISTOR_ALLOW_ACTIVE_OUTPUT_TESTS=true"
set "ERESISTOR_FIXTURE_CONFIRMATION=E_RESISTOR_SINGLE_CHANNEL_DMM"
```

Enable these only after verifying that the DMM is wired across the selected E-Resistor channel and that the other channels are not connected to a DUT.

## Results

Robot reports and extended evidence are written below:

```text
RegressionTest\results\robot\
```

Each run creates a timestamped directory containing `report.html`, `log.html`, `output.xml`, protocol transcripts, HIL measurements, coverage tables and the evidence checksum manifest.

## Pure Python runners

Native Python BAT launchers are also included. They do not require Robot Framework and write their evidence to `results\python`. See `WINDOWS_PYTHON_BAT_RUNNERS.md`.
