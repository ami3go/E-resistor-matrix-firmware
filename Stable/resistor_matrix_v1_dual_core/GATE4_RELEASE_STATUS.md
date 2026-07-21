# Gate 4 release status

- Firmware: **0.7.0**
- Gate: **G4**
- Feature: **two-phase eight-channel profile switching**
- Status: **implementation complete; target build and HIL acceptance pending**

## Acceptance requirements

- Corrected Gate 3 v0.6.1 production and fault-injection profiles pass.
- All eight persistent calibration files are present.
- Gate 4 profile diagnostics are coherent.
- Exactly one global break-before-make operation is counted per profile command.
- 1,000 zero-profile cycles finish with zero transport/profile errors.
- Profile transition p95 improves by at least 30% relative to the corrected Gate 3 baseline.
- Concurrent snapshot observation never reports an intermediate mixed software state.
- Fault injected after the global clear phase leaves all eight masks zero and the DMM confirms isolation.
