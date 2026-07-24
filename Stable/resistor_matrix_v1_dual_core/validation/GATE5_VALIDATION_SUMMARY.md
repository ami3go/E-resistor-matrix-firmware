# Gate 5 offline validation summary

| Check | Result |
|---|---:|
| Gate 5 structural source checks | 38/38 PASS |
| C/C++ lexical checks | 48/48 PASS |
| Focused host syntax checks | 6/6 PASS |
| Service/parser oracle | 10/10 PASS |
| Shared calibration bundle compile guard | PASS |
| Arduino-Pico target build | NOT RUN in packaging environment |
| Physical HIL | PENDING on bench |

Package revision `v0.8.0-r1` corrects the cross-translation-unit visibility of
`CALIBRATION_BUNDLE_MAX_BYTES`. Firmware protocol identity remains `0.8.0` so
RegressionTest v2.8.1 remains compatible.

A UI-only corrective also centers the Manual channel control bit indicator labels by using flex-centered circular button styling in the flash-backed CSS.

Gate 5 remains open until the corrected firmware compiles in Arduino IDE and all
required Robot Framework profiles pass on the accepted board using the pinned
Arduino-Pico 5.6.1 core.


## r3 rectangular bit-control UI corrective

Offline checks rerun after the web-control CSS correction:

- C/C++ lexical check: 48/48 PASS
- Gate 5 structural check: 38/38 PASS
- Gate 5 service oracle: 10/10 PASS

Arduino-Pico target compilation and browser screenshot validation must still be performed on the hardware/browser bench.

## r6 calibration readback layout corrective

Offline checks rerun after moving the Calibration file readback workflow from the Files tab to the Calibration tab:

- C/C++ lexical check: 48/48 PASS
- Gate 5 structural check: 38/38 PASS
- Gate 5 service oracle: 10/10 PASS

Arduino-Pico target compilation and browser validation must still be performed on the hardware/browser bench.

## r11 Ethernet cable auto-recovery corrective

Offline checks rerun after adding recoverable cable-late startup and link monitoring:

- C/C++ lexical check: 48/48 PASS
- Gate 5 structural check: 48/48 PASS
- Gate 3 focused host syntax check: PASS
- Gate 4 focused host syntax check: PASS

Arduino-Pico target compilation and cable-late/disconnect/reconnect HIL validation remain required.
