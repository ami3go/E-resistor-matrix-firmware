# Gate 5 r11 /state latency corrective

Firmware candidate: `v0.8.0-r11`

## Reason

Connected `G5-gate5_service` evidence from 2026-07-23 still failed only `G5-006` because `/state` p95 latency was 111.997 ms while the Gate 4 +5% limit was 103.266 ms. Heap drift was 0 B, so the problem was response size/transfer latency, not leakage.

## Changes

- Made default `/state` a fast compatibility payload that keeps the legacy keys required by existing regression tools.
- Preserved the previous verbose diagnostic payload at `/state?full=1` and `/state?verbose=1`.
- Increased `HttpResponseWriter::BUFFER_SIZE` from 4096 to 8192 bytes so common streamed responses fit in one buffered write.
- Changed the default `/state` status line to a derived runtime status: `ready`, `not_ready`, `outputs_not_safe`, or `safe_state`. This avoids keeping transient identify LED messages as the plain-text state status.
- Updated web asset cache busting to `ui=statefast-r11`.

## Expected validation impact

- Targeted fix for `G5-006 /state p95` latency.
- No change to resistor output logic, calibration math, SCPI command parsing, safety policy, or Core 1 transport behavior.
- Rerun `G5-gate5_service` after flashing.
