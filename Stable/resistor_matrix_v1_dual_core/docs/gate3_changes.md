# Optimization Gate 3 changes — firmware v0.6.0

Gate 3 makes Core 1 the deterministic and authoritative resistor-output engine. Core 0 retains networking, HTTP/SCPI parsing, files, calibration editing, UI rendering, and human-readable logging.

## Ownership boundary

Core 1 exclusively owns:

- shift-register data, clock, latch, and clear GPIO operations;
- authoritative channel masks and apply counters;
- break-before-make output transitions;
- final mask/safety validation immediately before GPIO access;
- direct physical all-OFF execution;
- active immutable calibration/safety policy;
- atomic publication of output snapshots.

Core 0 exclusively owns:

- Ethernet, HTTP, SCPI, USB text formatting, and LittleFS;
- mutable calibration and safety configuration;
- target-resistance calculation;
- command submission and bounded result waiting;
- evidence-oriented diagnostics and web/SCPI presentation.

Core 1 production files contain no Arduino `String`, networking classes, filesystem access, web services, USB serial output, or human-readable formatting.

## Bounded command transport

`core_transport_types.h` defines allocation-free numeric messages:

- command queue depth: 8;
- result queue depth: 8;
- event queue depth: 32;
- normal command timeout: 1000 ms;
- Core 0 heartbeat fail-safe threshold: 10 s.

Every command carries:

- a non-zero monotonic sequence number;
- an absolute microsecond deadline;
- a safety-policy generation;
- a numeric command type and fixed-size payload.

Core 1 rejects expired or generation-invalid commands before dispatching any physical operation. Core 0 invalidates the generation and requests direct all-OFF if a result timeout occurs.

## Immutable policy handoff

Calibration and safety configuration are copied into one of two fixed policy slots only when the coherent output snapshot reports:

- Core 1 ready;
- outputs safe;
- all eight masks equal to zero.

Staging advances the safety generation. Core 1 accepts the policy only through a sequenced `CORE_CMD_INSTALL_POLICY` command and then copies it into its active immutable policy.

## Coherent snapshots

Core 1 publishes one locked `CoreOutputSnapshot` containing:

- snapshot and last-command sequence numbers;
- active policy generation;
- flags for safe outputs, engine ready, fault, and installed policy;
- all eight masks;
- all eight apply counters.

HTTP and SCPI read this snapshot instead of assembling output state from independent shared variables.

## Fail-safe paths

- At Core 1 startup, the shift-register hardware is initialized and physically forced OFF before Core 0 starts network services.
- Normal `ALL:OFF` is a sequenced Core 1 command and does not invalidate a healthy policy.
- A Core 0 command timeout invalidates the generation and requests the direct Core 1 all-OFF path.
- If Core 0 stops refreshing its heartbeat while an output is active, Core 1 invalidates the generation and physically forces all channels OFF without depending on Core 0.

## Diagnostics

Production commands:

```text
SYST:CORE:TRANSPORT?
SYST:CORE:SNAPSHOT?
```

The first reports sequence and transport error counters. The second reports one coherent output snapshot.

## Test-image hooks

The following commands exist only when the firmware is compiled with `ERESISTOR_TEST_MODE=1`:

```text
SYST:TEST:MODE?
SYST:TEST:CORE1:INVALIDATE:NEXT
SYST:TEST:CORE1:DELAY <1..5000 ms>
SYST:TEST:CORE1:PAUSE <1..5000 ms>
SYST:TEST:CORE1:INVALIDATE
```

All mutation hooks require all eight outputs OFF. `INVALIDATE:NEXT` advances the transport generation inside Core 1 after the next command is dequeued but before envelope validation, proving that a queued stale command cannot reach GPIO dispatch. `DELAY` is used to prove that a Core 0 timeout followed by late Core 1 processing cannot create ghost actuation.

Test-image hooks are absent from the production build. Reflash the production image after fault-injection testing.
