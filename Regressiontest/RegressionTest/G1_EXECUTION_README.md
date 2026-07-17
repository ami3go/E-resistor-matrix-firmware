# Gate G1 Regression Execution

Use firmware **0.4.6** and run all three profiles against the archived G0 baseline.

## Required sequence

1. Flash and verify firmware 0.4.6.
2. Run `run_python_read_only.bat` or the Robot equivalent with `GATE=G1`.
3. Run `run_python_safe_output.bat`.
4. Confirm the single-channel DMM fixture, then run `run_python_hil_single_channel.bat`.
5. Use the included baseline: `baselines\G0_hil_single_channel_20260716T123002Z\results.json`.
6. Archive the complete G1 output directory.

## New Gate G1 assertions

- Oversized SCPI input produces only the overflow error and the next `*IDN?` succeeds.
- `/state` includes Core 1 stack, loop, event, and event-drop fields.
- HIL COM evidence contains structured `EVT` lines.
- Core 1 source paths contain no production Serial calls.
- `forceAllOff` is a boolean verified operation.

## Recommended Windows configuration

Add these entries to `robot_framework\variables\bench_config.local.bat`:

```bat
set "ERESISTOR_GATE=G1"
set "ERESISTOR_BASELINE=%REGRESSION_ROOT%\baselines\G0_hil_single_channel_20260716T123002Z\results.json"
set "ERESISTOR_GATE_MANIFEST=%REGRESSION_ROOT%\gate_acceptance.json"
set "ERESISTOR_SOURCE_DIR=C:\path\to\resistor_matrix_v1_dual_core"
```

The HIL runner still requires the existing active-output fixture confirmation.
