# RP2040 W5500 Resistor Matrix Firmware Description

## Purpose

This firmware controls an RP2040-Zero based programmable resistor matrix. Each of the eight output channels drives sixteen MOSFET-switched resistor branches through cascaded shift-register logic. The firmware provides Ethernet control through a browser UI and a raw TCP SCPI server on port 5025.

The current implementation is an Arduino RP2040 prototype with a safety-oriented dual-core split:

- **Core 0** runs non-real-time services: Ethernet, HTTP, SCPI, LittleFS, HTML generation, command parsing, profiles, and status reporting.
- **Core 1** owns all physical resistor-output operations: shift-register data, clock, SRCLR, latch pins, break-before-make switching, all-OFF, and channel mask shadow state.

## Hardware pinout

The pinout is unchanged from the previous firmware revisions.

| Function | GPIO |
|---|---:|
| W5500 MISO | GP0 |
| W5500 CS | GP1 |
| W5500 SCK | GP2 |
| W5500 MOSI | GP3 |
| W5500 INT | GP4 |
| CH1 latch | GP5 |
| CH2 latch | GP6 |
| CH3 latch | GP7 |
| CH4 latch | GP8 |
| CH5 latch | GP9 |
| CH6 latch | GP10 |
| Shift-register DATA | GP11 |
| Shift-register CLOCK | GP12 |
| CH7 latch | GP13 |
| CH8 latch | GP14 |
| Shift-register SRCLR | GP15, active low |
| WS2812 status LED | GP16 |

## Mask mapping

Each channel uses a 16-bit mask.

| Mask bit | MOSFET branch |
|---:|---|
| 0 | Q16, lowest resistance branch |
| 1 | Q15 |
| ... | ... |
| 15 | Q1, highest resistance branch |

The firmware shifts bit 0 first and bit 15 last. Logic `1` turns the MOSFET branch ON. Logic `0` turns it OFF. Safe OFF is mask `0x0000`.

## Safe switching sequence

A channel mask change is executed only by Core 1:

1. Latch `0x0000` to the selected channel.
2. Wait the configured break-before-make delay.
3. Shift the new mask, bit 0 first.
4. Pulse only the selected channel latch.
5. Update the shadow mask after the physical sequence completes.

## Boot sequence

Core 1 initializes the shift-register pins, pulses SRCLR low, latches all channels OFF, and reports `core1OutputsReady`. Core 0 waits for this readiness before starting Ethernet, HTTP, and SCPI services. This keeps outputs safe even if the W5500 or network stack fails.

## Safety model

HTTP and SCPI handlers shall not directly call low-level shift-register functions. They use wrappers such as `applyChannelMask()`, `applyAllMasksSafely()`, and `forceAllOff()`, which submit commands to Core 1 and wait with a bounded timeout.

`emergencyOffRequested` is a volatile flag checked by Core 1 outside the normal command queue, so all-OFF can bypass normal queue congestion.

## Calibration table strategy

The RP2040 still contains firmware-side nearest-mask search for browser/manual operation. However, SCPI calibration-table query commands were added so a PC driver can download the branch values and perform nearest-mask calculation on the PC side, avoiding slow exhaustive search on the microcontroller during high-throughput tests.
