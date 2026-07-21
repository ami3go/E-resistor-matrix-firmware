# Gate 3 execution sequence

## Production build acceptance

1. Flash E-Resistor firmware 0.6.1 built without `ERESISTOR_TEST_MODE`.
2. Confirm every output is OFF before connecting the DMM.
3. Run from PowerShell:

```powershell
.\run_robot_read_only.bat G3
.\run_robot_safe_output.bat G3
.\run_robot_hil_single_channel.bat G3
```

Gate 3 production acceptance requires all three profiles to pass, final masks `0000` on CH1-CH8, physical OFF isolation, no transport error-counter increments, no queue overflow, coherent snapshots, and physical resistance error below the configured limit.

## Fault-injection acceptance

After production acceptance, flash the special firmware made by `build_and_flash_COM17_gate3_test.bat`. Then run:

```powershell
.\run_robot_gate3_transport_fault.bat G3
```

The profile requires explicit fixture authorization and starts with all outputs OFF. It performs two independent safety proofs:

1. `INVALIDATE:NEXT` advances the generation inside Core 1 after command dequeue but before validation; the queued mask command must return an error without timeout or actuation.
2. A bounded delay holds the next command beyond the Core 0 timeout; the late command must be rejected by deadline or generation without actuation.

Both cases verify all eight masks and DMM isolation. The fault tests invalidate the installed policy generation. Reboot and flash the normal production image before any further operation.
