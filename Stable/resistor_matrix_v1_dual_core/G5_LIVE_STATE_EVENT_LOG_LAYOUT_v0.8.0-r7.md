# G5 Live State Event Log Layout Corrective v0.8.0-r7

## Purpose

Move the diagnostic event-log information out of the separate Log tab and into the Live State page so runtime, live plain-text state, and event-log status can be reviewed on one diagnostic screen.

## Changes

- The Live State page now has a three-column top layout:
  - left: Runtime / Memory table,
  - middle: Live plain-text `/state` output,
  - right: Event log summary and export action.
- The full Event history block is now shown at the bottom of the Live State page.
- The visible navigation no longer exposes a separate Log tab.
- `/log` remains available as a compatibility page and links to `/live` plus `/log_download`.
- `/log_download` is unchanged and still exports the same text log.
- Static asset URLs and ETag now use `ui=eventlive-r7` to avoid stale browser CSS/JS.

## Scope

This is a web-layout change only. It does not alter output masks, resistance calculation, Core 1 hardware control, SCPI command handling, HTTP API mutation routes, calibration file format, or event-log storage.

## Offline checks

- C/C++ lexical check: 48/48 PASS
- Gate 5 structural check: 38/38 PASS
- Gate 5 service oracle: 10/10 PASS

## Target validation required

After flashing, open `/live` and verify:

- Runtime / Memory appears in the left column.
- Live plain-text state appears in the middle column and updates every 2 seconds.
- Event log summary appears in the right column.
- Event history appears at the bottom of the Live State page.
- Log export still works from the Event log summary and from `/log_download`.
