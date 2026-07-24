# Gate 4 RegressionTest release status

- Package: **2.7.2**
- Firmware target: **0.7.2**
- Accepted baseline: **G3 / firmware 0.6.2**
- Gate: **G4**
- Status: **offline validated; connected target/HIL acceptance pending**

## Gate 4 cases

- `G4-001`: SCPI transport, SCPI profile query, and HTTP state expose coherent profile diagnostics.
- `G4-002`: 1,000 profile transitions, one BBM per profile, zero transport-error deltas, and at least 30% internal p95 improvement against the bundled G3 baseline.
- `G4-003`: concurrent state observation exposes only complete old/new eight-channel tuples; final DMM isolation passes.
- `G4-FI-001`: failure injected after the global clear phase returns an error, advances failure/BBM counters, leaves all masks zero, and passes physical isolation.
