# Gate 5 r12 /state schema and latency corrective

Connected `G5-gate5_service` evidence from 2026-07-23 showed that r11 fixed API JSON but introduced a default `/state` schema regression and still missed the `/state` p95 latency limit by approximately 1.49 ms.

## Corrections

- Restored all Robot-required Core 1, target-search, snapshot, and core-transport diagnostic keys in the default `/state` response.
- Kept the clean derived runtime status values: `ready`, `not_ready`, `outputs_not_safe`, and `safe_state`.
- Reworked the default `/state` path to build the plain-text body in a fixed static buffer and send it with `Content-Length` instead of chunked transfer.
- Preserved the verbose diagnostic view at `/state?full=1` and `/state?verbose=1`.
- Updated static asset cache token to `ui=statecompat-r12`.

## Expected validation impact

- `HTTP-002` should pass because the required schema keys are restored.
- `G5-006` should improve because default `/state` no longer uses chunked transfer.
- No resistor calculation, safety, calibration, SCPI command, or Core 1 output logic was changed.
