/**
 * @file hardware_config.h
 * @brief Pin and timing constants shared by Core 0 diagnostics and the Core 1 output engine.
 */
#pragma once

#include <stdint.h>
#include "board_config.h"

inline constexpr uint8_t ETH_MISO = 0;
inline constexpr uint8_t ETH_CS   = 1;
inline constexpr uint8_t ETH_SCK  = 2;
inline constexpr uint8_t ETH_MOSI = 3;
inline constexpr uint8_t ETH_INT  = 4;

inline constexpr uint8_t SR_DATA  = 11;
inline constexpr uint8_t SR_CLOCK = 12;
inline constexpr uint8_t SR_RESET = 15;
inline constexpr uint8_t SR_LATCH_PINS[CHANNEL_COUNT] = {5, 6, 7, 8, 9, 10, 13, 14};

inline constexpr uint8_t WS2812_PIN = 16;
inline constexpr uint8_t WS2812_COUNT = 1;

inline constexpr uint32_t ETH_INIT_SPI_HZ    = 1000000UL;
inline constexpr uint32_t ETH_RUNTIME_SPI_HZ = 4000000UL;
inline constexpr uint32_t ETH_LINK_POLL_INTERVAL_MS = 1000UL;
inline constexpr uint32_t ETH_HARDWARE_RETRY_INTERVAL_MS = 5000UL;
inline constexpr uint16_t SR_CLOCK_HALF_PERIOD_US = 10;
inline constexpr uint16_t SR_LATCH_PULSE_US       = 10;
inline constexpr uint16_t BREAK_BEFORE_MAKE_MS    = 2;

inline constexpr uint16_t HTTP_TCP_PORT = 80;
inline constexpr uint16_t SCPI_TCP_PORT = 5025;
