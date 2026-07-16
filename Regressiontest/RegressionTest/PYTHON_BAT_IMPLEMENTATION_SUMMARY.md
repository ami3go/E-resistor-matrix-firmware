# Pure Python BAT Implementation Summary

Package version: **2.3.4**

The package now includes native Windows BAT launchers that execute `run_regression.py` directly without using Robot Framework.

## Added launchers

- `setup_python_environment.bat`
- `run_python_read_only.bat`
- `run_python_safe_output.bat`
- `run_python_hil_single_channel.bat`
- `run_python_source_build.bat`
- `run_python_all_safe.bat`
- `run_python_custom.bat`
- `validate_python_harness.bat`

## Additional changes

- Existing `.venv` environments without pip are repaired using `python -m ensurepip --upgrade`.
- Python and Robot setup launchers share one setup implementation.
- Native Python results are written to timestamped directories under `results\python`.
- The Python and Robot runners share `robot_framework\variables\bench_config.local.bat`.
- HIL BAT execution retains the explicit active-output and fixture-confirmation interlocks.
- Pipe-delimited BAT values are supported for DMM initialization commands and USB serial fault patterns.
- Package version advanced to 2.3.4 while the archive root remains exactly `RegressionTest/`.

## Validation

- Python unit and package-layout tests: **23/23 passed**.
- Native Python offline execution: **passed**.
- Python source compilation: **passed**.
- CRLF validation for BAT files: **passed**.
- Windows execution was not performed because this environment does not provide `cmd.exe`.

## v2.3.4 reliability update

- Every BAT launcher now performs automatic environment validation and repair.
- Broken or moved virtual environments are deleted and recreated safely.
- Missing pip is repaired with `ensurepip`; stale `%errorlevel%` exits inside parenthesized CMD blocks were removed.
- Setup state is captured in `results\setup\last_setup_diagnostics.txt`.
- Native runs create `diagnostics.json`, `diagnostics.md`, lifecycle events, and a `RUN_INCOMPLETE` marker.
- Output-capable runs add verified, retrying final cleanup result `SAFE-999`.
- DMM auto-discovery closes every probed VISA resource even when `*IDN?` fails.
