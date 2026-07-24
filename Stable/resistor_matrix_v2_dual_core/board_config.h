/**
 * @file board_config.h
 * @brief Fixed E-Resistor hardware dimensions and shared default branch values.
 *
 * Mask bit 0 controls Q16 and mask bit 15 controls Q1. Runtime calibration is
 * stored numerically in channelResistorOhms[channel][bit]. The MOSFET name is
 * derived from the fixed mapping and is not duplicated per channel.
 */

#pragma once

#include <stdint.h>

inline constexpr uint8_t CHANNEL_COUNT = 8;
inline constexpr uint8_t BIT_COUNT = 16;

/** Shared nominal defaults. All channels start from this table before optional LittleFS calibration is loaded. */
inline constexpr float DEFAULT_RESISTOR_OHMS[BIT_COUNT] = {
  626.0f,
  1240.0f,
  2500.0f,
  5000.0f,
  10000.0f,
  20000.0f,
  40200.0f,
  80600.0f,
  160000.0f,
  324000.0f,
  643000.0f,
  1270000.0f,
  2490000.0f,
  5100000.0f,
  10000000.0f,
  20000000.0f
};
