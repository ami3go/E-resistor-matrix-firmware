# E-Resistor Gate 3 HIL regression plan

## Bench topology

- E-Resistor firmware 0.7.2 reachable by Ethernet HTTP/SCPI.
- RP2040 USB CDC connected for structured diagnostic capture.
- One USB/VISA DMM connected across exactly one selected channel.
- The other seven channels remain OFF for the complete physical run.

## Safety authorization

Active HIL is blocked unless both values are present in `bench_config.local.bat`:

```bat
set "ERESISTOR_ALLOW_ACTIVE_OUTPUT_TESTS=true"
set "ERESISTOR_FIXTURE_CONFIRMATION=E_RESISTOR_SINGLE_CHANNEL_DMM"
```

The selected channel and DMM/COM identities are written to `hardware_manifest.json`.

## Production image sequence

```powershell
.\run_robot_read_only.bat G3
.\run_robot_safe_output.bat G3
.\run_robot_hil_single_channel.bat G3
```

The HIL suite verifies all 16 single bits, configured parallel combinations, at least 50 repeated ON/OFF cycles, deterministic USB serial output, final DMM isolation, and final zero masks on all channels.

## Gate 3 fault image sequence

After normal profiles pass, flash the compile-time test image and run:

```powershell
.\run_robot_gate3_transport_fault.bat G3
```

The profile performs two all-OFF fault cases. First, Core 1 invalidates the next already-dequeued command before envelope validation; generation rejection must advance without a Core 0 timeout. Second, Core 1 delays a command beyond the Core 0 wait limit; the timeout must advance and the late command must be rejected by deadline or generation. Software masks and DMM isolation must remain OFF in both cases. Reboot and reflash the production image after this test.

## Acceptance limits

- Physical resistance absolute error: no more than the configured limit, default 1%.
- OFF resistance: at least the configured isolation limit, default 50 MOhm.
- Transport command/result queue overflows: zero.
- Normal-profile command timeouts, expired commands, generation rejects, invalid commands, and Core 0 fail-safe events: zero.
- Snapshot masks: identical between HTTP and SCPI.
- Final masks: CH1-CH8 all `0000`.
- Evidence manifest verification: pass.


## Gate 5 service-stress extension

Gate 5 runs the complete selected-channel bit, combination, and repeatability sequence while a background HTTP worker continuously alternates `/api/v1/state` and `/api/v1/diagnostics`. The firmware supports one SCPI client at a time, so SCPI stress is measured through the active HIL control connection: every verified `STATE?` transaction is counted while the stress interval is active. `G5-HIL-001` starts the interval and `G5-HIL-002` stops it and requires zero HTTP errors plus at least 20 successful HTTP and SCPI polls.
