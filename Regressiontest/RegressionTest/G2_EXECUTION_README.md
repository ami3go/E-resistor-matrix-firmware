# Gate 2 execution

Use firmware **0.5.0**. Build and flash it, then run:

```bat
run_python_read_only.bat G2
run_python_safe_output.bat G2
run_python_hil_single_channel.bat G2
```

Robot Framework equivalents are also available. Gate 2 is closed only after SCPI-006, HIL-008, source checks, and the complete single-channel HIL suite pass.
