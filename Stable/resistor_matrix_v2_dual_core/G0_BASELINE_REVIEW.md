# G0 HIL Baseline Review

Input: `G0-hil_single_channel-20260716T123002Z.zip`

## Result

- Robot Framework: **22 passed, 0 failed, 0 skipped**.
- Total suite duration: about **256 seconds**.
- Ethernet HTTP, SCPI, calibration, files, log export, stress, DMM and switching tests passed.

## Key measurements

| Metric | G0 value |
|---|---:|
| HTTP `/ping` p95 | 100.03 ms |
| HTTP `/state` p95 | 99.07 ms |
| SCPI `*IDN?` p95 | 217.75 ms |
| Concurrent HTTP `/state` p95 | 129.75 ms |
| Concurrent SCPI state p95 | 232.57 ms |
| Heap decline after 100 state requests | 0 bytes |
| Single-bit apply p95 | 228.28 ms |
| Single-bit maximum absolute error | 0.64434% |
| Single-bit average absolute error | 0.06165% |
| Combination maximum absolute error | 0.00746% |
| Repeated switching maximum absolute error | 0.000521% |
| Repeatability stdev | 0.0000345% |
| Final all-off measured resistance | Open / infinity |

## Findings carried into G1

1. The baseline is suitable for Gate G1 because all required G0 functions passed.
2. The SCPI oversized-line response contained both an overflow and an undefined-header error, confirming the line-tail parsing defect.
3. The COM monitor captured zero serial lines, so G1 must generate structured events during active operations and the next run must verify their presence.
4. DMM settling dominates the HIL duration: typical settling was roughly 4.6–5.0 seconds, with one branch reaching 10.77 seconds. This is not treated as a firmware regression by itself.
5. Physical resistance accuracy is comfortably inside the current 1% gate limit.
