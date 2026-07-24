# Performance History

## Nearest-mask search

The original prototype iterated all 65,535 non-zero masks and reparsed text resistance values for active bits. The optimized implementation introduced fixed-popcount enumeration using Gosper's method, cached conductance values, and an inline safety bound. With a four-active-bit limit it visits 2,516 masks instead of 65,535.

Gate 2 replaced the per-candidate logarithm with conductance cross-product ranking and archived deterministic mask/target oracle results.

## Gate 3 transport timing

Gate 3 assigns a deadline to every Core 0/Core 1 command and records submitted/completed sequences plus timeout, expiry, generation-reject, and queue-overflow counters. Production regression requires zero deltas for these error counters. The special test image deliberately creates one queued-generation rejection and one timeout/late-rejection case while proving that no physical output appears.
