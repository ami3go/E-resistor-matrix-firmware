# Regression Harness Update v2.5.2

This patch corrects defects found while replaying `G0-hil_single_channel-20260717T114044Z.zip` against firmware 0.5.0.

## Corrected defects

1. HTTP state lines such as `ch1=0x0000 ...` now parse as valid channels.
2. SCPI state fields such as `CH1=0x0000,OPEN` now parse as valid channels.
3. `STATE?` is retried up to three times when a TCP response is incomplete; every failed parse records channel count and a bounded raw excerpt.
4. Known firmware/gate combinations are checked. Firmware 0.5.0 is Gate G2. A mismatched gate blocks active HIL fixture initialization.
5. `LOG-001` now requires a non-empty plain-text E-Resistor event log with firmware identity and event-history markers.
6. `RUN_INCOMPLETE` is removed before checksums, a `RUN_COMPLETE` marker is created, the final event is flushed, and then the evidence manifest is generated.
7. The manifest is immediately re-read and verified. Results are stored in `evidence_manifest_verification.json`.
8. Pure-Python runs now produce the same completion markers and manifest verification as Robot Framework runs.
9. `environment.json` includes the regression package version and Robot library version.
10. Gate 2 BAT launchers default to G2 so an old persistent `ERESISTOR_GATE=G0` does not silently skip Gate 2 coverage.

## Replay validation

The failed report was replayed with the corrected parsers:

- HTTP channel count: 8
- Every captured SCPI `STATE?` response: 8 channels
- The old manifest was correctly detected as invalid because `events.jsonl` changed after hashing and `RUN_INCOMPLETE` was removed after hashing.

See `validation/v2.5.2_failed_report_replay.json`.

## Required rerun

```bat
run_robot_read_only.bat G2
run_robot_safe_output.bat G2
run_robot_hil_single_channel.bat G2
```

The HIL runner will refuse to activate the selected channel when a known firmware version does not match the selected optimization gate.

## v2.5.2 packaging note

Arduino CLI integration and firmware compilation checks are not included. Offline source checks remain available through the `source_check` profile.
