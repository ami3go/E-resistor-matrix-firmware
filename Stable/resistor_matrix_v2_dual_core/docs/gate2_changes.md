# Optimization Gate 2 changes

Firmware version: **0.5.0**

## Numeric calibration model

The previous runtime entry stored bit number, MOSFET text, and resistance text for every channel branch. Gate 2 stores only numeric resistance values in `channelResistorOhms[channel][bit]`. The bit index is implicit and the common MOSFET name is returned by `mosfetNameForBit()`. Text parsing and formatting occur only at import, export, SCPI, and web boundaries.

This reduces the estimated model allocation from approximately 3,200 bytes of entry text plus the 512-byte conductance cache to 512 bytes of resistance values plus the existing 512-byte conductance cache: an estimated 2,688-byte static RAM saving. Exact linker output is the acceptance authority.

## Target search

The fixed-popcount search remains, but candidate ranking no longer calls `logf()`. For candidate conductance `g`, target conductance `gT`, the relative resistance error is proportional to `abs(gT-g)/g`. Candidate ratios are compared by cross multiplication.

The search records candidates and elapsed microseconds, checks a deadline/cancellation request every 256 candidates, and services Core 0 progress functions at those checkpoints.

## New SCPI commands

```text
CH<n>:TARGET:CALC? <ohm>
SYST:DIAG:SERIAL?
```

The target command is read-only. It returns:

```text
requested_ohm=...,mask=....,calculated_ohm=...,absolute_error_ohm=...,error_percent=...,candidates=...,elapsed_us=...
```

The serial diagnostic command returns `OK,SERIAL_TEST` over SCPI and emits an `EVT ... code=SERIAL_TEST` line on USB CDC.

## Compatibility

The following formats are unchanged:

- `/ch1.csv` through `/ch8.csv`
- Calibration bundle BEGIN/END blocks
- `CAL:RES?` compact response
- Header-initializer import support
- Existing channel mask and target-apply commands
