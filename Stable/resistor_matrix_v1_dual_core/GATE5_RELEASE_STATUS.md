# Gate 5 release status

## Candidate

- Firmware identity: **v0.8.0**
- Package revision: **r6 Calibration readback layout corrective**
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


## r2 service corrective

- `/api/v1/channels` now emits `null` for open/non-finite resistance instead of non-standard `inf`, so the endpoint remains valid JSON.
- `HttpResponseWriter` now uses a 4096-byte static stream buffer. The legacy `/state` response remains streamed but normally flushes in one buffered content write, reducing the Gate 5 service latency regression while avoiding large per-request stack usage.
- Offline checks were rerun after the service corrective: C/C++ lexical 48/48 PASS, Gate 5 structural 38/38 PASS, Gate 5 service oracle 10/10 PASS.


## r3 web control UI corrective

- Manual control bit indicators are now rectangular 28 x 24 px rounded buttons instead of circular controls.
- Bit numbers are centered using CSS `inline-grid` with `place-items:center`.
- Bit-button form wrappers no longer use inline layout style; layout is controlled by `http_static_assets.cpp`.
- Static asset URLs now include `ui=rectbit-r3` and the CSS/JS ETag includes the same revision suffix, preventing browsers from reusing older cached `app.css?v=0.8.0`.
- This change is UI-only and does not alter output masks, resistance calculation, Core 1 hardware control, SCPI, HTTP API state mutation, or calibration storage.
- Offline checks after r3: C/C++ lexical 48/48 PASS, Gate 5 structural 38/38 PASS, Gate 5 service oracle 10/10 PASS.


## r4 Live State runtime layout corrective

- The runtime monitor table has been moved into the Live State page.
- The Live State page is now split into two responsive columns: runtime table on the left and `Live plain-text state` on the right.
- The visible navigation no longer exposes a separate Runtime tab; `/runtime` remains available as a compatibility page with a link to `/live`.
- Channel masks remain below the live two-column section.
- Static asset URLs now include `ui=livestate-r4` and the CSS/JS ETag includes the same revision suffix, preventing browsers from reusing older cached assets.
- This change is UI-only and does not alter output masks, resistance calculation, Core 1 hardware control, SCPI, HTTP API mutation, or calibration storage.
- Offline checks after r4: C/C++ lexical 48/48 PASS, Gate 5 structural 38/38 PASS, Gate 5 service oracle 10/10 PASS.
## r5 SCPI identify LED update

- `*IDN?` now triggers the 5-second bright-blue identify LED blink.
- SCPI identity response text remains unchanged for driver compatibility.
- Requires target validation after flashing: send `*IDN?` and confirm LED identify pattern plus unchanged response.

## r6 Calibration readback layout corrective

- The **Calibration file readback** card has been moved from the Files tab to the Calibration tab.
- The Calibration tab now contains the all-channel calibration backup, file list, and restore/import controls next to the active calibration table view.
- The Files tab now focuses on LittleFS capacity and file inventory only, with a notice pointing users to the Calibration tab for readback/import.
- Successful all-channel calibration import now redirects to `/settings?cal_import=ok` so the confirmation appears on the Calibration tab.
- Static asset URLs and ETag now use `ui=calreadback-r6` to avoid stale browser assets.
- This is a web-layout change only; output masks, resistance calculation, Core 1 hardware control, SCPI, and calibration file format are unchanged.
- Offline checks after r6: C/C++ lexical 48/48 PASS, Gate 5 structural 38/38 PASS, Gate 5 service oracle 10/10 PASS.


## r7 Live State event log layout corrective

- The event log summary has been moved from the separate Log tab to the third column of the Live State page.
- The Live State top section is now a three-column responsive layout: Runtime / Memory, Live plain-text state, and Event log.
- The full Event history block is now shown at the bottom of the Live State page.
- The visible navigation no longer exposes a separate Log tab; `/log` remains as a compatibility page linking to `/live` and `/log_download`.
- Static asset URLs and ETag now use `ui=eventlive-r7` to avoid stale browser assets.
- This is a web-layout change only; output masks, resistance calculation, Core 1 hardware control, SCPI, and calibration file format are unchanged.
- Offline checks after r7: C/C++ lexical 48/48 PASS, Gate 5 structural 38/38 PASS, Gate 5 service oracle 10/10 PASS.


## v0.8.0-r8 Files / Backup layout update

- Backup export and factory-reset controls moved into the Files tab.
- Visible Backup navigation tab removed.
- `/backup` retained as a compatibility page that redirects operator workflow to `/files`.
- Factory reset completion now returns to `/files`.

## v0.8.0-r9 UI corrective — Profiles moved to Control

- Moved visible profile management workflow to the Control page.
- Removed the Profiles tab from top navigation.
- Replaced `/profiles` content with a compatibility notice linking to Control.
- Removed the redundant Current state table from the former Profiles page.
- Cache-bust token: `ui=profilecontrol-r9`.
