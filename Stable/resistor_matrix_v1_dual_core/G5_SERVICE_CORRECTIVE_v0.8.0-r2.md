# Gate 5 service corrective — v0.8.0-r2

## Reported failures

Connected `G5-gate5_service` evidence showed two remaining failures:

1. `/api/v1/channels` emitted non-standard JSON for open outputs:

   ```json
   "resistance_ohm":inf
   ```

   JSON does not support `inf`; open or non-finite resistance values must be represented as `null`.

2. Legacy `/state` p95 latency regressed versus the Gate 4 baseline because the 3.4 KB state response was split into many 512-byte chunks.

## Corrections

- `handleApiV1Channels()` now writes `null` unless the calculated resistance is finite and positive.
- `HttpResponseWriter` now uses a 4096-byte static response buffer. This keeps the common legacy `/state` payload in a single buffered content write while avoiding a large per-request stack object.
- The change preserves the streamed response model used by large pages and downloads.
- Gate 4 Core 1 transport and physical-output engine sources remain unchanged.

## Expected validation impact

- `G5-002 API V1 Schema And Compatibility Aliases` should pass because `/api/v1/channels` is valid JSON and returns 8 channel objects.
- `G5-006 Thousand-Page Heap And State Latency` should improve because `/state` no longer requires multiple small 512-byte chunks in the normal case.

## Validation performed here

Offline source checks only:

- C/C++ lexical checks: 48/48 PASS
- Gate 5 structural checks: 38/38 PASS
- Gate 5 service oracle: 10/10 PASS

Arduino-Pico target build and connected `G5-gate5_service` regression must still be rerun on the hardware bench.
