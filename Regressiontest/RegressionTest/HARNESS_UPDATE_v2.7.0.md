# RegressionTest v2.7.0 changes

- Inherits the Gate 3 v2.6.2 DTR and calibration-persistence corrections.
- Recognizes firmware 0.7.0 as Gate G4.
- Adds `gate4_profile` and `gate4_profile_fault` Robot profiles.
- Adds G4 diagnostics, 1,000-cycle stress, baseline performance comparison, coherent snapshot observation, and failure-after-clear HIL.
- Requires a corrected Gate 3 `results.json` for the 30% internal p95 comparison.
- Keeps the fixed internal ZIP root `RegressionTest/`.
