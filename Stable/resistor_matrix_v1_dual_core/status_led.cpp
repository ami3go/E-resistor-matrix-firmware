/**
 * @file status_led.cpp
 * @brief WS2812 heartbeat and fault/status indication logic.
 */

#include "app.h"

// 04_status_led.ino
// Split from rp2040_w5500_resistor_matrix_v1_safety_logic.ino.
// Keep all files in the same Arduino sketch folder.

// ============================================================
// WS2812 heartbeat
// ============================================================

/**
 * @brief Ws2812 Set.
 * @param r Function parameter.
 * @param g Function parameter.
 * @param b Function parameter.
 */
void ws2812Set(uint8_t r, uint8_t g, uint8_t b) {
  heartbeatPixel.setPixelColor(0, heartbeatPixel.Color(r, g, b));
  heartbeatPixel.show();
}

/**
 * @brief Initialize the WS2812 heartbeat pixel.
 */
void heartbeatBegin() {
  heartbeatPixel.begin();
  heartbeatPixel.setBrightness(24);
  ws2812Set(0, 0, 8);
}

/**
 * @brief Set Led Mode.
 * @param mode Function parameter.
 */
void setLedMode(LedMode mode) {
  ledMode = mode;
  lastLedUpdateMs = 0;
}

/**
 * @brief Update the WS2812 indicator according to the current LED mode.
 */
void updateHeartbeat() {
  uint32_t now = millis();

  uint16_t intervalMs = 700;

  if (ledMode == LED_ACTIVITY) {
    intervalMs = 120;
  } else if (ledMode == LED_FAULT) {
    intervalMs = 250;
  } else if (ledMode == LED_BOOT) {
    intervalMs = 400;
  }

  if (now - lastLedUpdateMs < intervalMs) {
    return;
  }

  lastLedUpdateMs = now;
  ledPhase = !ledPhase;

  if (!ledPhase) {
    ws2812Set(0, 0, 0);
    return;
  }

  switch (ledMode) {
    case LED_BOOT:
      ws2812Set(0, 0, 16);
      break;

    case LED_OK:
      ws2812Set(0, 18, 0);
      break;

    case LED_ACTIVITY:
      ws2812Set(18, 12, 0);
      break;

    case LED_FAULT:
    default:
      ws2812Set(24, 0, 0);
      break;
  }
}

