# Regression harness correction v2.7.0

- Asserts USB CDC DTR while capturing RP2040 serial output. The previous harness explicitly deasserted DTR, which caused Arduino-Pico USB CDC writes to be suppressed even though COM17 opened successfully.
- Keeps RTS low and avoids the 1200-baud bootloader trigger.
- Adds `CAL-001` so a gate run stops with a clear calibration-persistence failure when `/ch1.csv` ... `/ch8.csv` are missing or empty.
- Restores explicit Gate 3 transport/snapshot/stress tests in the Robot production suites.
- Internal ZIP root remains `RegressionTest/`.

## Added after G3 report review

- USB CDC monitor asserts DTR before reading RP2040 events.
- `CAL-001` blocks physical regression when persistent calibration files are missing.
- Firmware 0.6.1 is accepted as Gate G3.
- `G3-004` records the eight-channel zero-profile latency baseline required for Gate 4 comparison.
