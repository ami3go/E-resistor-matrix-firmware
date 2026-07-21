# Regression harness update v2.7.0

- Converted the release surface to Robot Framework only.
- Removed pure-Python BAT launchers, source/build Robot workflows, and Arduino CLI execution from the regression package.
- Added firmware 0.6.0 to the G3 gate identity map.
- Added G3 transport diagnostics, coherent-snapshot, safe-output stress, and test-build no-ghost-actuation checks.
- Added separate queued-generation invalidation and Core 0 timeout/late-command fault cases so generation and deadline rejection are independently proven.
- Raised G3 single-channel HIL repeatability to at least 50 cycles.
- Added a dedicated fault-injection Robot suite and guarded Windows launcher.
- Preserved parser fixes for `0x`-prefixed masks and bounded SCPI state retries.
- Preserved completion-before-hash evidence finalization and manifest self-verification.
- Kept software version capture for the package, Robot library, Robot Framework, Python, host, and device when reachable.
