# E-Resistor Optimization Gate Package


## Stable archive folder name

The downloadable ZIP filename includes descriptive keywords and a version number, but the directory inside the ZIP is always:

```text
RegressionTest/
```

This stable root name is intentional. New versions can be extracted over the existing `RegressionTest` working folder without changing scripts, CI paths, virtual-environment commands, or bench configuration references.

This package contains:

- `OPTIMIZATION_GATE_TASK.md` — implementation-ready gate plan.
- `e_resistor_regression/` — standard-library Python regression harness.
- `test_config.example.json` — example runtime configuration.
- `gate_acceptance.example.json` — per-gate thresholds and source expectations.

## Safe first run

```bash
python run_regression.py \
  --host 192.168.0.55 \
  --gate G0 \
  --profile read_only \
  --output results/G0
```

The regression harness and Windows launchers default to `192.168.0.55`. Override `--host` when the device uses another address.

## Safe command-path test

This profile sends only zero masks and `ALL:OFF`:

```bash
python run_regression.py \
  --host 192.168.0.55 \
  --gate G1 \
  --profile safe_output \
  --allow-output-tests \
  --output results/G1
```

## Source and optional Arduino CLI build checks

```bash
python run_regression.py \
  --host 192.168.0.55 \
  --gate G1 \
  --profile read_only \
  --source-dir ../resistor_matrix_v1_dual_core \
  --arduino-cli arduino-cli \
  --fqbn rp2040:rp2040:rpipico \
  --output results/G1
```

Use the actual board FQBN installed on the development system.

## Compare with a previous accepted gate

```bash
python run_regression.py \
  --host 192.168.0.55 \
  --gate G2 \
  --profile read_only \
  --baseline results/G1/results.json \
  --output results/G2
```

## Generated files

- `run.log`
- `events.jsonl`
- `http_transcript.log`
- `scpi_transcript.log`
- `metrics.csv`
- `results.json`
- `junit.xml`
- `report.md`
- `state_before.txt`
- `state_after.txt`
- `environment.json`
- `diagnostics.json` and `diagnostics.md`
- `RUN_INCOMPLETE` while a run is executing or when it terminates unexpectedly
- `source_metrics.json` when `--source-dir` is used
- `build.log` when Arduino CLI build is enabled

## Safety

- `read_only` does not intentionally change outputs or persistent configuration.
- `safe_output` requires `--allow-output-tests` and sends only zero masks.
- Active-output, storage, OTA, and watchdog tests are intentionally not enabled by a single generic flag.
- The runner retries `ALL:OFF`, verifies all eight masks are `0000`, and records `SAFE-999` after output tests.
- A failed software all-off response is reported as a test failure; it is not proof of physical output state without hardware feedback.

## CI/offline source-only run

```bash
python run_regression.py \
  --gate G1 \
  --profile read_only \
  --source-dir ../resistor_matrix_v1_dual_core \
  --gate-manifest gate_acceptance.example.json \
  --skip-device \
  --output results/G1-source
```

## Harness self-test

```bash
python -m unittest discover -s tests -v
```

## Test coverage table

The package includes `TEST_COVERAGE_TABLE.md`, which maps optimization requirements to:

- Gate and required regression profile.
- Automated or HIL test ID.
- Required hardware.
- Acceptance rule.
- Evidence files retained with the gate result.
- Current implementation state: Implemented, Planned, or Manual.

Every regression execution now also creates a gate-filtered live coverage table:

- `test_coverage.md` — human-readable coverage and gaps.
- `test_coverage.csv` — spreadsheet-friendly traceability table.
- `test_coverage.json` — machine-readable coverage result.

Coverage states are `PASS`, `FAIL`, `PARTIAL`, `SKIP`, `NOT_RUN`, `PLANNED`, and `MANUAL`. A skipped, partial, planned, manual, or not-run requirement is not treated as fully covered for gate acceptance.

## Robot Framework runner

The package now includes a complete `robot_framework/` implementation that exposes the same regression IDs as individual Robot tests.

Install:

```bash
python -m pip install -r requirements-robot.txt
python -m pip install -e .
```

Read-only run:

```bash
python robot_framework/run_robot.py \
  --profile read_only \
  --gate G0 \
  --host 192.168.0.55
```

Single-channel hardware run:

```bash
python robot_framework/run_robot.py \
  --profile hil_single_channel \
  --gate G0 \
  --host 192.168.0.55 \
  --allow-active-output-tests \
  --fixture-confirmation E_RESISTOR_SINGLE_CHANNEL_DMM \
  --serial-port COM7 \
  --dmm-resource "USB0::...::INSTR" \
  --hil-channel 1
```

Robot creates `output.xml`, `log.html`, and `report.html`. The original E-Resistor transcripts, HIL CSV, metrics, coverage reports, and baseline comparison are written below the Robot output directory in `e_resistor_evidence/<profile>/`.

See `robot_framework/README.md` and `robot_framework/docs/ROBOT_TEST_MAPPING.md`.

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


## Windows pure-Python BAT launchers

The native Python harness can also be run directly from BAT files:

```bat
setup_python_environment.bat
run_python_read_only.bat G0
run_python_safe_output.bat G0
run_python_hil_single_channel.bat G0
run_python_source_build.bat G0
run_python_all_safe.bat G0
```

These runners use the same `robot_framework\variables\bench_config.local.bat` as the Robot runners and store results under `results\python`. See `WINDOWS_PYTHON_BAT_RUNNERS.md`.
