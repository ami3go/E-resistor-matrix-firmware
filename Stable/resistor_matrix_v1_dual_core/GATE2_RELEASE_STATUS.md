# Gate 2 Release Status

Firmware target: **v0.5.0**

## Source implementation

Gate 2 implements the numeric calibration model and target-search optimization while retaining all external calibration formats and the Gate 1 Core 1 safety boundary.

### Completed

- Numeric `channelResistorOhms[8][16]` runtime model.
- Common MOSFET mapping stored once.
- One shared default 16-branch table.
- Direct bit indexing; no runtime linear search.
- Conductance cache retained for fast parallel-resistance calculations.
- No per-candidate logarithm or division in target ranking.
- Search candidate, elapsed-time, timeout, and cancellation telemetry.
- Read-only target calculation SCPI command.
- Deterministic USB serial diagnostic command.
- Host oracle: 8,136 mask vectors and 704 target vectors passed.
- Gate 2 source checks: 14/14 passed.
- Estimated model RAM saving: 2,688 bytes.

### Hardware gate-exit evidence still required

1. Build using Arduino CLI 1.5.1, Arduino-Pico 5.6.1, 200 MHz, 1 MB sketch + 1 MB LittleFS, IPv4-only 32K, `-Os`, Pico SDK USB, exceptions/RTTI disabled.
2. Record sketch, static RAM, ELF, BIN, and UF2 sizes.
3. Run G2 read-only, safe-output, and single-channel HIL profiles.
4. Require SCPI-006 and HIL-008 to pass.
5. Require `core1_event_drop_count=0`, queue overflow count zero, all final masks zero, and DMM final isolation PASS.
6. Compare physical accuracy, apply latency, heap, and protocol latency to the accepted Gate 1 baseline.
7. Measure normal and expert target-search execution time on the RP2040.

Gate 2 source implementation is complete. Gate 2 remains open until the build and real-hardware evidence pass.
