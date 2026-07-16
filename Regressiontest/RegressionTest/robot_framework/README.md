# E-Resistor Robot Framework Regression

This folder runs the same E-Resistor regression logic through Robot Framework while preserving the original Python evidence and coverage outputs.

Robot produces its standard files:

- `output.xml`
- `log.html`
- `report.html`

The adapter additionally writes the existing E-Resistor evidence under:

```text
<robot-output>/e_resistor_evidence/<profile>/
```

That directory contains SCPI, HTTP, DMM and serial transcripts, HIL measurements, metrics, source/build results, baseline comparison, coverage reports and a SHA-256 evidence manifest.

## Folder layout

```text
robot_framework/
├── libraries/
│   └── e_resistor_robot_library.py
├── listeners/
│   └── evidence_listener.py
├── resources/
│   ├── common.resource
│   └── source_build.resource
├── suites/
│   ├── read_only.robot
│   ├── safe_output.robot
│   ├── hil_single_channel.robot
│   └── source_build.robot
├── variables/
│   └── bench.py
├── profiles/
│   ├── read_only.args
│   ├── safe_output.args
│   └── hil_single_channel.args
├── scripts/
│   ├── setup_venv.ps1
│   ├── setup_venv.sh
│   └── generate_libdoc.py
├── ci/
│   └── validate_robot_suites.py
├── run_robot.py
├── run_robot.ps1
└── run_robot.sh
```

## Installation

### Windows PowerShell

```powershell
cd RegressionTest
.\robot_framework\scripts\setup_venv.ps1
.\.venv\Scripts\Activate.ps1
```

### Linux

```bash
cd RegressionTest
./robot_framework/scripts/setup_venv.sh
source .venv/bin/activate
```

A vendor VISA runtime is recommended for the USB DMM. `PyVISA-py` can be used for supported resources when no vendor runtime is available.

## Run read-only regression

```powershell
python .\robot_framework\run_robot.py `
  --profile read_only `
  --gate G0 `
  --host 192.168.0.55
```

This profile never requests a nonzero resistance mask.

## Run safe-output regression

```powershell
python .\robot_framework\run_robot.py `
  --profile safe_output `
  --gate G1 `
  --host 192.168.0.55 `
  --allow-output-tests
```

This profile only exercises zero-mask and repeated `ALL:OFF` operations.

## Run real-hardware HIL regression

```powershell
python .\robot_framework\run_robot.py `
  --profile hil_single_channel `
  --gate G2 `
  --host 192.168.0.55 `
  --allow-active-output-tests `
  --fixture-confirmation E_RESISTOR_SINGLE_CHANNEL_DMM `
  --serial-port COM7 `
  --dmm-resource "USB0::0x0957::0x0607::MY12345678::INSTR" `
  --hil-channel 1 `
  --hil-bits 0-15 `
  --hil-error-limit-percent 1.0
```

Required wiring:

```text
PC Ethernet ───────────── E-Resistor SCPI port 5025
PC USB ────────────────── RP2040 USB CDC/COM diagnostic port
USB DMM resistance input ─ selected E-Resistor channel
```

The selected channel is active only during its measurement. Every mask application verifies through SCPI that all other channels remain `0000`. `ALL:OFF` cleanup runs after every Robot test and again during suite teardown.

## Run source and build checks

```powershell
python .\robot_framework\run_robot.py `
  --profile source_build `
  --gate G2 `
  --source-dir C:\path\to\resistor_matrix_v1_dual_core `
  --arduino-cli arduino-cli `
  --fqbn "rp2040:rp2040:waveshare_rp2040_zero:flash=2097152_1048576"
```

When `--fqbn` is omitted, the Arduino build test is skipped while source checks still run.

## Bench configuration through environment variables

The variable file reads `ERESISTOR_*` environment variables. Examples:

```powershell
$env:ERESISTOR_HOST = "192.168.0.55"
$env:ERESISTOR_SERIAL_PORT = "COM7"
$env:ERESISTOR_DMM_RESOURCE = "USB0::...::INSTR"
$env:ERESISTOR_HIL_CHANNEL = "1"
```

Command-line options used by `run_robot.py` set these variables automatically for the child Robot process.

## Run Robot directly

From the project root:

```powershell
$env:ERESISTOR_HOST = "192.168.0.55"
python -m robot `
  --pythonpath . `
  --outputdir .\results\robot-direct `
  .\robot_framework\suites\read_only.robot
```

For direct safe-output or HIL execution, set the corresponding safety variables first:

```powershell
$env:ERESISTOR_ALLOW_ACTIVE_OUTPUT_TESTS = "true"
$env:ERESISTOR_FIXTURE_CONFIRMATION = "E_RESISTOR_SINGLE_CHANNEL_DMM"
```

## Select individual tests by tag

```powershell
python .\robot_framework\run_robot.py `
  --profile hil_single_channel `
  --gate G2 `
  --host 192.168.0.55 `
  --allow-active-output-tests `
  --fixture-confirmation E_RESISTOR_SINGLE_CHANNEL_DMM `
  --include HIL-003
```

Be careful when selecting a dependent HIL test alone. `HIL-003` to `HIL-007` require `HIL-001` and `HIL-002` to initialize and verify the fixture. Normally run the complete HIL suite.

## Output structure

```text
results/robot/G2-hil_single_channel-<timestamp>/
├── output.xml
├── log.html
├── report.html
├── robot_events.jsonl
└── e_resistor_evidence/hil_single_channel/
    ├── environment.json
    ├── events.jsonl
    ├── run.log
    ├── http_transcript.log
    ├── scpi_transcript.log
    ├── serial_transcript.log
    ├── serial_console.log
    ├── dmm_transcript.log
    ├── hardware_manifest.json
    ├── hil_measurements.csv
    ├── hil_summary.json
    ├── metrics.csv
    ├── results.json
    ├── report.md
    ├── test_coverage.md
    ├── test_coverage.csv
    ├── test_coverage.json
    ├── legacy_junit.xml
    └── evidence_manifest.sha256
```

## Safety behavior

- Active tests are disabled unless explicit flags are present.
- HIL requires the exact fixture confirmation string.
- The library verifies all eight software masks after every active-mask command.
- `ALL:OFF` is attempted after each active test.
- `ALL:OFF` is attempted again during suite finalization.
- DMM OFF isolation is checked before and after the HIL sequence.
- Failure of fixture discovery or the safe-state precheck causes dependent HIL tests to skip.

The current firmware reports `OK` for `ALL:OFF` even in some internal failure paths identified by the code review. Therefore, the HIL DMM isolation checks remain an important independent safety verification.

## CI and syntax validation

After installing requirements:

```powershell
python .\robot_framework\ci\validate_robot_suites.py
```

This performs a Robot Framework dry run and does not contact hardware.

## Keyword documentation

```powershell
python .\robot_framework\scripts\generate_libdoc.py
```

The generated HTML is written to:

```text
robot_framework/docs/EResistorRobotLibrary.html
```

## Design principle

The Robot adapter does not duplicate instrument or protocol logic. It calls the same `RegressionSuite` methods as `run_regression.py`. This ensures a failure has the same acceptance criteria, metrics and evidence regardless of which runner is used.

## Windows BAT launchers

Windows users can run the test profiles directly from the fixed `RegressionTest` root:

```bat
setup_robot_environment.bat
run_robot_read_only.bat G0
run_robot_safe_output.bat G0
run_robot_hil_single_channel.bat G0
run_robot_source_build.bat G0
run_robot_all_safe.bat G0
```

Bench-specific values are stored in `robot_framework\variables\bench_config.local.bat`. See `WINDOWS_BAT_RUNNERS.md` for setup, safety authorization, and launcher details.

