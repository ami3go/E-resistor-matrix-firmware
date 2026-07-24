# RegressionTest v2.7.2 changes

- Targets corrected Gate 4 firmware v0.7.2, based on accepted Gate 3 v0.6.2.
- Preserves firmware-to-gate mapping for v0.6.2 and adds v0.7.2 as G4.
- Bundles the accepted G3 safe-output profile baseline for board `503359277A981F9F`.
- `run_robot_gate4_profile.bat G4` now uses the bundled baseline automatically.
- Keeps Robot Framework as the only user-facing regression workflow.
- Includes G4 diagnostics, 1,000-cycle profile stress, coherent snapshot observation, and failure-after-clear physical isolation.
- Evidence continues to record regression package/library, Robot Framework, Python, device identity, firmware version, and SHA-256 verification.
