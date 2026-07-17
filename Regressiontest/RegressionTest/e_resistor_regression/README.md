# E-Resistor Regression Harness

The package uses only the Python standard library for HTTP, SCPI TCP, reports, source checks, and build orchestration.

The `hil_single_channel` profile additionally uses:

- `pyserial` for RP2040 USB CDC/COM capture.
- `PyVISA` for the USB-controlled DMM.

## Modules

- `clients.py` — HTTP and SCPI clients with transcripts.
- `hil.py` — COM monitor, configurable VISA DMM, stability logic, equivalent-resistance calculations, and HIL CSV output.
- `parsers.py` — firmware state and calibration parsers.
- `suite.py` — protocol, performance, safety, and HIL tests.
- `source_checks.py` — architecture and legacy-pattern scans.
- `reports.py` — JSON/JUnit/CSV/Markdown outputs and baseline comparison.
- `logging_ext.py` — detailed structured and protocol logging.
- `cli.py` — command-line entry point.

## Safety

The HIL profile does not run solely because it was selected. It also requires:

```text
--allow-active-output-tests
--fixture-confirmation E_RESISTOR_SINGLE_CHANNEL_DMM
```

It commands only the selected channel, verifies the other seven masks remain zero, and performs all-off cleanup between test groups and at shutdown.

## Coverage outputs

The runner evaluates the static coverage registry after every run and writes:

- `test_coverage.md`
- `test_coverage.csv`
- `test_coverage.json`

The master matrix is available at the package root as `TEST_COVERAGE_TABLE.md`. Coverage evaluation is profile-aware and does not count missing HIL, OTA, watchdog, or manual evidence as passed.
