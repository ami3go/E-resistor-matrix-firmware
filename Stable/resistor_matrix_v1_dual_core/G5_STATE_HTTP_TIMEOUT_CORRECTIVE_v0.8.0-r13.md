# Gate 5 r13 /state timeout corrective

Connected `G5-gate5_service` evidence from 2026-07-23 showed that r12 caused all Gate 5 service tests to time out. The r12 default `/state` path used `server.setContentLength()`, `server.send(..., "")`, then `server.sendContent(...)`; on the RP2040/W5500 WebServer stack this can leave clients waiting for a response/body and blocks the service tests.

## Changes

- Kept the restored default `/state` schema keys required by `HTTP-002`.
- Replaced the r12 WebServer fixed-length send sequence with a direct `WiFiClient` HTTP response writer for `/state`.
- The response now emits explicit `Content-Length`, no-cache headers, and the full body in one raw client write.
- Preserved `/state?full=1` and `/state?verbose=1` verbose streaming behavior.

## Expected validation impact

- `HTTP-002` should no longer time out and should keep the required schema.
- `G5-001`..`G5-006` should no longer fail due to global HTTP/SCPI timeout caused by `/state`.
- `/state` remains optimized without using chunked transfer in the default fast path.
