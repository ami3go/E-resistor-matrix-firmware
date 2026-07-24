# Regression harness v2.8.1

Gate 5 setup and offline-validation corrective release. Firmware requirements and Robot test logic are unchanged from v2.8.0.

## Corrected

- Replaced embedded backspace and vertical-tab control characters in `scripts/setup_windows_environment.bat` with literal Windows path separators.
- Correctly creates and reports `robot_framework\variables\bench_config.local.bat`.
- Correctly invokes `robot_framework\ci\validate_robot_suites.py`.
- Added explicit `ExtendedLogger.close()` lifecycle handling so `run.log` is released on Windows before temporary directories are deleted.
- Limited source CRLF validation to package-owned BAT files; `.venv`, build output, and installed third-party packages are excluded.
- Added a regression test that rejects non-printing control characters in the setup script.
- Improved setup error handling and Python-version validation.

## Compatibility

- Firmware target remains Gate 5 firmware `0.8.0`.
- Robot suites, test IDs, safety interlocks, and Gate 5 acceptance limits are unchanged.
- Existing `.venv` directories and `bench_config.local.bat` files are preserved.
