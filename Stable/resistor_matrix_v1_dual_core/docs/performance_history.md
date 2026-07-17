# Performance History

## Nearest-mask search

The original prototype iterated all 65,535 non-zero masks and reparsed text resistance values for active bits. The optimized implementation introduced fixed-popcount enumeration using Gosper's method, cached conductance values, and an inline safety bound. With a four-active-bit limit it visits 2,516 masks instead of 65,535.

Gate G2 will separately benchmark and replace the per-candidate logarithm only after a deterministic equivalence oracle has been archived.
