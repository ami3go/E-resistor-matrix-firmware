# Robot regression design

Robot Framework owns suite selection, lifecycle, test status, HTML/XML reports, tags, and timeouts. `EResistorRobotLibrary` owns initialization, evidence finalization, safety cleanup, and one-to-one dispatch from Robot test IDs to internal `RegressionSuite` methods.

## Safety boundary

- Read-only never requests output mutation.
- Safe-output is limited to zero masks and `ALL:OFF`.
- Single-channel HIL requires explicit authorization and fixture confirmation.
- Fault injection requires a compile-time test image, starts with outputs OFF, and must be followed by reboot/reflash.
- Every active profile performs best-effort all-off cleanup and captures final state.

## Evidence boundary

The final test event is flushed before manifest generation. `RUN_COMPLETE` replaces `RUN_INCOMPLETE`; all listed files are then hashed and immediately reverified. No evidence file is intentionally modified afterward.
