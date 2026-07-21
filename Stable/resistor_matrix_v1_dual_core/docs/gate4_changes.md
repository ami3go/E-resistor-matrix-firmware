# Optimization Gate 4 changes — firmware v0.7.0

Gate 4 replaces eight sequential per-channel break-before-make delays for an all-channel profile with one global two-phase transition.

The physical clear phase shifts zero once because all channel chains share data and clock, then pulses each independent latch. The make phase shifts each requested channel mask and pulses the corresponding latch. Core 0 sees one atomic snapshot only after the full make phase succeeds.

New diagnostics:

- `SYST:CORE:PROFILE?`
- profile fields in `SYST:CORE:TRANSPORT?`
- profile fields in `/state`

The test image adds `SYST:TEST:PROFILE:FAIL:NEXT`, which is accepted only while all outputs are OFF.
