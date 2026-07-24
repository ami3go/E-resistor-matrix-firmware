# Gate 5 web control bit button corrective update — v0.8.0-r3

## Purpose

The manual control page previously rendered the 16-bit channel indicators as small circular buttons. On some browsers/displays the bit numbers appeared visually off-center even after flex centering.

## Firmware/UI changes

- Changed bit indicators from circular 24 x 24 px buttons to rectangular 28 x 24 px buttons with rounded corners.
- Centered bit labels using `inline-grid` and `place-items:center`.
- Removed inline form styling from generated bit-button HTML so CSS fully controls layout.
- Added `line-height:0` to the bit-button container/form wrappers to remove baseline alignment artifacts.
- Added CSS URL cache-bust revision `ui=rectbit-r3` and matching asset ETag suffix so browsers do not reuse older cached `app.css?v=0.8.0`.

## Expected result

The manual channel-control table should show rectangular bit buttons with the bit numbers centered inside each button.

## Validation note

This is a web UI rendering correction only. It does not change resistance calculation, SCPI behavior, Core 1 output control, calibration storage, or safety logic.
