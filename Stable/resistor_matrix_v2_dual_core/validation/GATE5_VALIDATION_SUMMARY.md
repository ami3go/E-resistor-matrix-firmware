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
