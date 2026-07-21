# Gate 3 release status

Firmware target: **v0.6.1**  
Regression package: **v2.7.0**

Gate 2 acceptance is recorded from the user's completed bench run. Gate 3 source implementation is complete and awaiting production build plus real-hardware acceptance.

## Implemented

- Deterministic Core 1 ownership of shift-register GPIO and authoritative masks.
- Fixed-size command, result, and diagnostic event transport.
- Sequence, deadline, and safety-generation validation before hardware access.
- Semaphore-backed completion and startup synchronization.
- Atomic Core 1 output snapshots for HTTP and SCPI.
- Immutable double-buffered safety/calibration policy installation only while outputs are OFF.
- Direct Core 1 all-off path on Core 0 heartbeat loss.
- Production-disabled fault hooks and a separate test-build launcher.
- Robot-only G3 runtime, stress, snapshot, HIL, queued-generation, timeout, and no-ghost-actuation coverage.

## Offline status

Source, model-oracle, lexical, Python, package-layout, and static Robot validations are included under `validation/`. Arduino compilation and connected HIL are not represented as completed by this package build.

## Gate closure

Gate 3 closes only after the production read-only, safe-output, and single-channel HIL profiles pass, followed by a passing fault-injection profile using the special test image and a final reflash of the production image.
