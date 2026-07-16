# E-Resistor Single-Channel Hardware-in-the-Loop Regression Plan

## 1. Purpose

Use the real E-Resistor board, its Ethernet SCPI server, its USB serial diagnostic port, and a USB-controlled DMM to verify that each optimization gate preserves both software behavior and physical resistance accuracy.

This profile is designed for the available bench fixture where the DMM is connected to one E-Resistor output channel.

## 2. Bench topology

```text
Regression PC
├── Ethernet ─────────────── E-Resistor W5500
│                            └── SCPI TCP port 5025
├── USB serial/COM ───────── E-Resistor RP2040 USB CDC
└── USB/VISA ─────────────── DMM
                              ├── HI ── selected E-Resistor channel terminal A
                              └── LO ── selected E-Resistor channel terminal B
```

No energized DUT shall be connected during the active HIL profile.

The remaining seven E-Resistor channels may remain physically unconnected, but the test runner verifies that their software masks remain `0000` throughout every active measurement.

## 3. Supported instruments

The HIL adapter uses:

- `pyserial` for the RP2040 COM/USB CDC stream.
- `PyVISA` for the DMM.
- Generic SCPI commands for DMM configuration and measurement.

Default DMM commands are suitable for an HP/Keysight 34401A:

```text
*CLS
CONF:RES AUTO
TRIG:SOUR IMM
SAMP:COUN 1
READ?
```

Commands are configurable from the command line for other DMM models.

Install dependencies:

```bash
python -m pip install -r requirements-hil.txt
```

A vendor VISA implementation such as Keysight IO Libraries Suite or NI-VISA may be required for USB/GPIB adapters. `PyVISA-py` can be used where the connected interface is supported.

## 4. Safety interlocks

Active tests run only when both conditions are present:

```text
--allow-active-output-tests
--fixture-confirmation E_RESISTOR_SINGLE_CHANNEL_DMM
```

Before enabling any resistor branch, the runner:

1. Sends `ALL:OFF`.
2. Reads `STATE?` and confirms all eight masks are `0000`.
3. Starts USB serial capture.
4. Repeats and verifies `ALL:OFF` because opening a COM port can affect some boards.
5. Opens and identifies the DMM.
6. Downloads and validates all 16 calibration values for the selected channel.
7. Measures the selected channel in the OFF state.
8. Refuses active tests when OFF resistance is below the configured threshold.

During active tests:

- Only the selected channel may have a nonzero mask.
- Every mask command is verified using `STATE?`.
- Any state mismatch triggers immediate best-effort `ALL:OFF` cleanup.
- Every test group returns all channels OFF.
- Final cleanup runs even when a test throws an exception.

Software confirmation is not a substitute for shift-register feedback. The DMM supplies independent physical evidence only for the selected channel.

## 5. Automatic HIL tests

### HIL-001 — Instrument and interface discovery

Verifies:

- Board SCPI connection.
- RP2040 COM port.
- DMM VISA resource.
- DMM `*IDN?` identity.
- Selected channel calibration table.

### HIL-002 — Safe-state precheck

Verifies:

- All eight masks are `0000`.
- Selected channel measures as open or above `--hil-off-min-ohm`.

Default minimum OFF resistance:

```text
50 MOhm
```

Adjust this threshold if fixture leakage is characterized and documented.

### HIL-003 — Single-bit physical walk

For every selected bit:

1. Set exactly one mask bit.
2. Verify the selected channel mask.
3. Verify the other seven channels remain OFF.
4. Read DMM samples until stable or timeout.
5. Compare measured resistance with the firmware calibration value.
6. Record error, stability, settle time, SCPI time, and DMM time.
7. Return all outputs OFF.

Default bit selection:

```text
0-15
```

### HIL-004 — Combination-mask physical test

Measures configured parallel combinations using the channel calibration table as the expected value.

Default masks:

```text
0003,0005,0009
```

These defaults use two active branches and should normally remain above the default minimum-resistance limit. Confirm suitability with the actual channel calibration and safety settings.

### HIL-005 — Repeated switching

Repeatedly applies the first selected single-bit mask and returns to OFF.

Measures:

- Physical resistance accuracy per cycle.
- Resistance repeatability.
- Apply latency.
- All-off latency.
- Failure or timeout behavior.

Default cycles:

```text
10
```

Increase this for gate soak tests.

### HIL-006 — Serial fault inspection

Searches the captured RP2040 USB serial stream for configurable fault terms such as:

```text
fatal
panic
assert
hardfault
queue overflow
```

The full raw serial log is retained even when no fault pattern is found.

### HIL-007 — Final all-off isolation

After all active tests:

1. Send and verify `ALL:OFF`.
2. Measure the selected channel multiple times.
3. Confirm the OFF resistance remains above the configured threshold.

## 6. Measurement algorithm

For each active mask, the DMM is sampled until the latest measurement window satisfies:

```text
relative standard deviation <= configured stability limit
```

Defaults:

| Parameter | Default |
|---|---:|
| Stability window | 5 samples |
| Sample interval | 0.25 s |
| Minimum wait | 0.5 s |
| Settle timeout | 20 s |
| Stability limit | 0.20% |
| Resistance error limit | 1.00% |

The reported resistance is the median of the final stable window. The CSV also records mean, standard deviation, relative standard deviation, and all timing values.

## 7. Example run

Windows PowerShell:

```powershell
python .\run_regression.py `
  --host 192.168.0.55 `
  --gate G0 `
  --profile hil_single_channel `
  --allow-active-output-tests `
  --fixture-confirmation E_RESISTOR_SINGLE_CHANNEL_DMM `
  --serial-port COM7 `
  --dmm-resource "USB0::0x0957::0x0607::MY12345678::INSTR" `
  --dmm-idn-contains 34401 `
  --hil-channel 1 `
  --hil-bits 0-15 `
  --hil-combination-masks 0003,0005,0009 `
  --hil-repeat-cycles 10 `
  --hil-error-limit-percent 1.0 `
  --output .\results\G0-hil
```

Linux:

```bash
python run_regression.py \
  --host 192.168.0.55 \
  --gate G0 \
  --profile hil_single_channel \
  --allow-active-output-tests \
  --fixture-confirmation E_RESISTOR_SINGLE_CHANNEL_DMM \
  --serial-port /dev/ttyACM0 \
  --dmm-resource 'USB0::0x0957::0x0607::MY12345678::INSTR' \
  --dmm-idn-contains 34401 \
  --hil-channel 1 \
  --output results/G0-hil
```

Automatic discovery can be requested with:

```text
--serial-port auto
--dmm-resource auto
```

When multiple devices are connected, explicit resource names are preferred for reproducible gate results.

## 8. Gate-by-gate HIL policy

| Gate | Required physical tests |
|---|---|
| G0 | Full bit walk, combinations, repeatability baseline |
| G1 | Full HIL; compare switching latency and serial output behavior |
| G2 | Full HIL mandatory; validates numeric model and search refactor |
| G3 | Full HIL plus increased repeat cycles; validates dual-core transport |
| G4 | Full selected-channel HIL plus eight-channel state/profile software tests |
| G5 | Full HIL while concurrent HTTP and SCPI polling runs |
| G6 | Full HIL before and after calibration export/import restoration |
| G7 | Full HIL before OTA and after reboot; compare firmware identity and calibration hash |
| G8 | Full HIL plus watchdog fault tests; DMM must confirm selected channel becomes OFF |
| G9 | Full HIL, extended cycles, and 24-hour read-only/periodic physical soak |

A gate cannot be accepted when its required physical profile fails, even when all source and protocol-only tests pass.

## 9. Generated evidence

The HIL profile adds:

- `serial_console.log` — timestamped raw RP2040 USB serial lines.
- `serial_transcript.log` — structured serial events when used.
- `dmm_transcript.log` — every DMM request and response.
- `hil_measurements.csv` — one row per physical measurement.
- Standard SCPI and HTTP transcripts.
- `events.jsonl` with HIL measurement records.
- `state_before.txt` and `state_after.txt`.
- `results.json`, `metrics.csv`, `junit.xml`, and `report.md`.

Archive all these files with the firmware binary, source revision, board serial number, DMM identity, calibration backup, and test fixture description.

## 10. Acceptance rules

Unless a gate defines tighter limits:

- All selected bit measurements must stabilize.
- Every selected bit must remain within the configured error limit.
- Every configured combination must remain within the configured error limit.
- No unexpected mask may appear on another channel.
- All-off verification must pass after every test group.
- Final OFF resistance must exceed the configured threshold.
- No configured serial fault pattern may be present.
- No unexplained latency, heap, or physical-error regression may exceed the gate threshold.

## 11. Test coverage reporting

The master requirement-to-test mapping is maintained in `TEST_COVERAGE_TABLE.md`.

After every HIL run, the runner generates:

| File | Purpose |
|---|---|
| `test_coverage.md` | Gate-filtered coverage table with status and evidence |
| `test_coverage.csv` | Coverage data for spreadsheets and release tracking |
| `test_coverage.json` | Coverage data for CI and automated gate decisions |

A HIL run physically covers one selected channel. The table explicitly marks full CH1–CH8 physical verification as manual until the DMM is rewired for each channel or a relay/multiplexer fixture is installed. Software state and mask checks continue to cover all eight channels on every run.
