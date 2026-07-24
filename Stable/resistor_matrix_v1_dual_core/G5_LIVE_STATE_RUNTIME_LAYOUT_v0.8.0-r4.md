# G5 live-state runtime layout corrective — v0.8.0-r4

## Purpose

Move the runtime monitor table from the top-level Runtime tab into the Live State page, so live diagnostics are visible on one screen.

## UI changes

- The visible navigation no longer shows a separate Runtime tab.
- The `/runtime` route remains available as a compatibility page and links to `/live`.
- The `/live` page is split into two responsive columns:
  - Left: Runtime / Memory table previously shown on the Runtime page.
  - Right: Live plain-text state from `/state`, auto-polled every 2 seconds.
- Channel masks remain below the two-column live section.
- Static asset cache-busting changed to `ui=livestate-r4` and the CSS/JS ETag uses the same suffix.

## Scope

This is a web UI layout change only. It does not alter output masks, resistance calculation, calibration storage, SCPI command behavior, HTTP mutation semantics, or Core 1 hardware control.

## Offline validation

- C/C++ lexical check: 48/48 PASS
- Gate 5 structural check: 38/38 PASS
- Gate 5 service oracle: 10/10 PASS

Arduino-Pico target compilation and connected Robot Framework regression still need to be run on the hardware bench.
