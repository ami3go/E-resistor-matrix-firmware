# Windows Pure-Python BAT Test Runners

These launchers run the native `run_regression.py` harness directly, without Robot Framework. They use the same fixed `RegressionTest` root, `.venv`, and bench configuration as the Robot runners.

## First-time setup

Run:

```bat
setup_python_environment.bat
```

The setup script:

1. Creates or repairs `.venv`.
2. Restores pip with `ensurepip` when the environment was created without pip.
3. Installs the package with the HIL dependencies (`pyserial` and `PyVISA`).
4. Creates `robot_framework\variables\bench_config.local.bat` when missing.
5. Runs the Python self-tests.

The same local bench file is deliberately shared by the Python and Robot launchers, so IP, COM, DMM and source settings need to be maintained only once.

## Launchers

| BAT file | Purpose | Active resistance output |
|---|---|---:|
| `run_python_read_only.bat` | HTTP, SCPI, timing, stress, calibration and file checks | No |
| `run_python_safe_output.bat` | Read-only checks plus zero-mask and repeated `ALL:OFF` tests | No nonzero mask |
| `run_python_hil_single_channel.bat` | SCPI + USB serial + DMM physical regression | Yes, selected channel |
| `run_python_source_build.bat` | Source metrics and optional Arduino CLI build | No device access |
| `run_python_all_safe.bat` | Read-only, safe-output and configured source/build profiles | No nonzero mask |
| `run_python_custom.bat` | Pass arbitrary arguments to `run_regression.py` | Depends on arguments |
| `validate_python_harness.bat` | Unit tests and CLI validation | No |

Pass the gate as the first argument:

```bat
run_python_read_only.bat G2
run_python_safe_output.bat G1
run_python_hil_single_channel.bat G2
run_python_source_build.bat G3
run_python_all_safe.bat G3
```

## HIL safety authorization

The HIL launcher refuses to run until both settings are deliberately enabled in `robot_framework\variables\bench_config.local.bat`:

```bat
set "ERESISTOR_ALLOW_ACTIVE_OUTPUT_TESTS=true"
set "ERESISTOR_FIXTURE_CONFIRMATION=E_RESISTOR_SINGLE_CHANNEL_DMM"
```

Enable them only after confirming that the DMM is connected across the selected E-Resistor channel and that the other channels are disconnected from a DUT.

## Results

Each run creates a timestamped directory below:

```text
RegressionTest\results\python\<gate>\
```

The directory contains the native regression evidence, including `report.md`, `results.json`, `junit.xml`, protocol transcripts, metrics, coverage tables and HIL measurements when applicable.

## Direct custom example

```bat
run_python_custom.bat --profile read_only --gate G2 --host 192.168.0.55 --output results\python\manual_G2
```
