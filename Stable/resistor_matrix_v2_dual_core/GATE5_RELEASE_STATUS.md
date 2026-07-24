# Gate 5 release status

## Candidate

- Firmware identity: **v0.8.0**
- Package revision: **r1 compile corrective**
- Gate: **G5 — HTTP and SCPI restructuring**
- Accepted baseline: **v0.7.2 / Gate 4**
- Status: **PENDING ARDUINO-PICO TARGET BUILD AND CONNECTED HIL**

## Implemented

- Monolithic HTTP implementation split into focused control, calibration, maintenance, diagnostics, API, route, static-resource, and streaming-writer modules.
- Central application header split into focused interfaces; `app.h` is retained as a small compatibility umbrella.
- CSS and JavaScript moved to flash-backed immutable-cache assets.
- Calibration page, legacy `/state`, API responses, log export, and calibration/backup downloads use fixed-buffer streaming.
- One coherent runtime snapshot is captured before state formatting.
- Canonical `/api/v1/` health, state, channel, diagnostics, calibration, and mutation endpoints added.
- Legacy calibration aliases retained for one deprecation interval.
- Legacy mutation GET routes now reject with HTTP 405 and advertise POST.
- Exact SCPI commands use a fixed command registry and fixed-buffer normalization/channel parsing.
- SCPI HELP and the browser command table use the same registry.
- Gate 4 Core 1 transport and physical-output engine sources are unchanged.

## Offline validation

- Gate 5 structural checks: **38/38 PASS**
- C/C++ lexical checks: **48/48 PASS**
- Focused host C++ syntax checks: **6/6 PASS**
- Gate 5 service oracle: **10/10 PASS**

The r1 corrective moves `CALIBRATION_BUNDLE_MAX_BYTES` from a private `http_maintenance.cpp` definition into the shared `http_api.h` interface so `http_pages.cpp` and the import handler compile against the same limit. These checks do not replace Arduino-Pico compilation or hardware regression.

## r1 UI alignment corrective

- Manual control bit indicators now use inline-flex centering, zero inherited button padding/margins, and circular 24 px controls so bit numbers 15..0 remain centered in Chrome, Edge, and Firefox.
- This change is confined to flash-backed CSS in `http_static_assets.cpp`; firmware identity and HTTP/SCPI protocols remain unchanged.
