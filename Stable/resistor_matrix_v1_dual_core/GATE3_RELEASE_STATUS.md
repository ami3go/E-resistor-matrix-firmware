# Gate 3 firmware release status

Firmware target: **v0.6.0**

Gate 2 acceptance is recorded from the user's completed bench regression. Gate 3 source implementation is complete. Arduino target compilation and connected hardware acceptance are not claimed by this source package.

## Implemented

- Core 1 is the sole physical output engine and sole writer of Core 1 readiness/fault state.
- Fixed-size allocation-free command, result, and event queues.
- Sequence, deadline, and safety-generation checks before physical dispatch.
- Semaphore-backed command completion and startup synchronization.
- Immutable double-buffered calibration/safety policy installation while all outputs are OFF.
- Locked coherent output snapshots consumed by HTTP and SCPI.
- Direct Core 1 all-OFF response to Core 0 command timeout and heartbeat loss.
- Numeric Core 1 diagnostics with Core 0 text formatting.
- Production-disabled, output-OFF-interlocked fault-injection hooks.
- Separate production and Gate 3 test-image build launchers.

## Offline validation included

- Gate 3 structural source checks.
- Transport state-model oracle.
- C++ lexical validation.
- Host C++ syntax checks for the transport and Core 1 engine with Pico/Arduino stubs.

## Hardware gate exit

Gate 3 closes only after:

1. production firmware 0.6.0 builds and flashes;
2. Robot read-only, safe-output, and single-channel HIL profiles pass at G3;
3. transport queues do not overflow and normal-run timeout, expiry, generation-reject, invalid-command, and Core 0 fail-safe deltas remain zero;
4. HTTP and SCPI snapshots are coherent;
5. physical accuracy remains within the configured limit and final isolation passes;
6. the test-image queued-invalidation and timeout no-ghost-actuation cases pass;
7. the production image is reflashed and its identity verified.
