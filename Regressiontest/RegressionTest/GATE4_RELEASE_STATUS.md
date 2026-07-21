# Gate 4 RegressionTest release status

- Package: **2.7.0**
- Firmware target: **0.7.0**
- Gate: **G4**
- Status: **offline validated; connected HIL acceptance pending**

## New cases

- `G4-001`: profile diagnostics agree across SCPI transport, SCPI profile query, and HTTP state.
- `G4-002`: 1,000 zero-profile transitions; one BBM operation per transition; no error counters; internal p95 improves by at least 30% against `G3-004`.
- `G4-003`: concurrent state observation exposes only complete old/new profile tuples; final DMM isolation passes.
- `G4-FI-001`: failure injected after global clear produces an error, zero masks, one failure count, one BBM count, and physical isolation.
