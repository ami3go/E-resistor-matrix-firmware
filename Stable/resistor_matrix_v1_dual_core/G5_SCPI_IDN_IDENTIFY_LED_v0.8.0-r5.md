# Gate 5 r5 — SCPI *IDN? Identify LED Update

## Change

`*IDN?` now starts the same 5-second bright-blue WS2812 identify blink used by the web UI identify action.

## Compatibility

The SCPI response payload is unchanged:

```text
OpenBench,E-Resistor,<serial>,<firmware_version>
```

Only the physical status LED behavior changes. This keeps PC driver parsing compatible while allowing an operator to identify the board from any SCPI identity query.

## Implementation

`processScpiLine()` calls `startIdentifyLedBlink(5000U)` in the `ScpiCommandId::IdnQuery` branch before returning the identity string.

## Validation required

After flashing, verify:

1. Send `*IDN?` over SCPI TCP port 5025.
2. Confirm the response string is unchanged.
3. Confirm the WS2812 status LED blinks bright blue for approximately 5 seconds.
4. Confirm normal heartbeat mode resumes after the identify interval.
