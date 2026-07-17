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


## v2.5.1 harness corrections

- HTTP and SCPI channel masks accept both `0000` and `0x0000`.
- `STATE?` parsing uses bounded retries and records raw excerpts on failure.
- Firmware 0.5.0 must be run as Gate G2; a stale G0 bench setting blocks active HIL.
- Log export requires a non-empty E-Resistor text log, not merely HTTP 200.
- Robot and pure-Python evidence manifests are generated after final events and completion markers, then verified automatically.
- `environment.json` includes regression package and Robot library versions.
- Gate 2 BAT launchers default to G2; pass another gate explicitly only for intentional historical testing.

## Safe first run

```bash
python run_regression.py \
  --host 192.168.0.55 \
  --gate G2 \
  --profile read_only \
  --output results/G2
```

Gate G2 firmware v0.5.0 defaults to `192.168.0.55`. Override `--host` when the device uses another address.

## Safe command-path test

This profile sends only zero masks and `ALL:OFF`:

```bash
python run_regression.py \
  --host 192.168.0.55 \
  --gate G2 \
  --profile safe_output \
  --allow-output-tests \
  --output results/G2
```

## Source and optional Arduino CLI build checks

```bash
python run_regression.py \
  --host 192.168.0.55 \
  --gate G2 \
  --profile read_only \
  --source-dir ../resistor_matrix_v1_dual_core \
  --arduino-cli arduino-cli \
  --fqbn rp2040:rp2040:waveshare_rp2040_zero:flash=2097152_1048576 \
  --output results/G2
```

Use the actual board FQBN installed on the development system.

## Compare with a previous accepted gate

```bash
python run_regression.py \
  --host 192.168.0.55 \
  --gate G2 \
  --profile read_only \
  --baseline baselines/G1_hil_single_channel_20260717T103129Z/results.json \
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
- `RUN_COMPLETE`
- `evidence_manifest.sha256`
- `evidence_manifest_verification.json`
- `source_metrics.json` when `--source-dir` is used
- `build.log` when Arduino CLI build is enabled

## Safety

- `read_only` does not intentionally change outputs or persistent configuration.
- `safe_output` requires `--allow-output-tests` and sends only zero masks.
- Active-output, storage, OTA, and watchdog tests are intentionally not enabled by a single generic flag.
- The runner performs best-effort `ALL:OFF` cleanup after output tests.
- A failed software all-off response is reported as a test failure; it is not proof of physical output state without hardware feedback.

## CI/offline source-only run

```bash
python run_regression.py \
  --gate G2 \
  --profile read_only \
  --source-dir ../resistor_matrix_v1_dual_core \
  --gate-manifest gate_acceptance.example.json \
  --skip-device \
  --output results/G2-source
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
  --gate G2 \
  --host 192.168.0.55
```

Single-channel hardware run:

```bash
python robot_framework/run_robot.py \
  --profile hil_single_channel \
  --gate G2 \
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
run_robot_read_only.bat G2
run_robot_safe_output.bat G2
run_robot_hil_single_channel.bat G2
run_robot_source_build.bat G2
run_robot_all_safe.bat G2
```

Bench-specific values are stored in `robot_framework\variables\bench_config.local.bat`. See `WINDOWS_BAT_RUNNERS.md` for setup, safety authorization, and launcher details.


## Windows pure-Python BAT launchers

The native Python harness can also be run directly from BAT files:

```bat
setup_python_environment.bat
run_python_read_only.bat G2
run_python_safe_output.bat G2
run_python_hil_single_channel.bat G2
run_python_source_build.bat G2
run_python_all_safe.bat G2
```

These runners use the same `robot_framework\variables\bench_config.local.bat` as the Robot runners and store results under `results\python`. See `WINDOWS_PYTHON_BAT_RUNNERS.md`.

## Gate 2 additions

Gate 2 adds SCPI-006 read-only target-mask oracle testing, HIL-008 deterministic USB serial verification, numeric-model source checks, target-search telemetry, and an accepted Gate 1 hardware baseline. Firmware 0.5.0 is expected for G2 execution.
