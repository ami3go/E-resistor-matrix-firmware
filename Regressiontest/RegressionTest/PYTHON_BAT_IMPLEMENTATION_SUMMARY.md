# Pure Python BAT Implementation Summary

Package version: **2.5.2**

The package now includes native Windows BAT launchers that execute `run_regression.py` directly without using Robot Framework.

## Added launchers

- `setup_python_environment.bat`
- `run_python_read_only.bat`
- `run_python_safe_output.bat`
- `run_python_hil_single_channel.bat`
- `run_python_source_check.bat`
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
- Package version advanced to 2.5.2 while the archive root remains exactly `RegressionTest/`.

## Validation

- Python unit and package-layout tests: **36/36 passed**.
- Native Python offline source-check execution: **passed**.
- Python module syntax/bytecode compilation: **passed**.
- CRLF validation for BAT files: **passed**.
- Windows execution was not performed because this environment does not provide `cmd.exe`.
