/**
 * @file status_led.cpp
 * @brief WS2812 heartbeat, fault/status, and device-identification indication logic.
 */

#include "app.h"

// 04_status_led.ino
// Split from rp2040_w5500_resistor_matrix_v1_safety_logic.ino.
// Keep all files in the same Arduino sketch folder.

// ============================================================
// WS2812 heartbeat
// ============================================================

/**
 * @brief Write one RGB color to the onboard WS2812 status pixel.
 * @param r Red intensity, 0..255.
 * @param g Green intensity, 0..255.
 * @param b Blue intensity, 0..255.
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
  heartbeatPixel.setBrightness(32);
  ws2812Set(0, 0, 8);
}

/**
 * @brief Set the normal status LED mode.
 * @param mode New LED mode.
 */
void setLedMode(LedMode mode) {
  ledMode = mode;
  lastLedUpdateMs = 0;
}

/**
 * @brief Start a temporary bright-blue identify blink pattern.
 *
 * This is used from the web UI when multiple PCBs are connected and the
 * operator needs to identify which physical board belongs to the current
 * browser session. The previous LED mode is restored automatically after
 * the requested duration.
 *
 * @param durationMs Duration of the identify pattern in milliseconds.
 */
void startIdentifyLedBlink(uint32_t durationMs) {
  if (ledMode != LED_IDENTIFY_BLUE) {
    ledModeBeforeIdentify = ledMode;
  }

  ledMode = LED_IDENTIFY_BLUE;
  ledIdentifyUntilMs = millis() + durationMs;
  ledIdentifyPatternStep = 0;
  lastLedUpdateMs = 0;
  ledPhase = false;
  ws2812Set(0, 0, 0);
}

/**
 * @brief Update the WS2812 indicator according to the current LED mode.
 */
void updateHeartbeat() {
  uint32_t now = millis();

  if (ledMode == LED_IDENTIFY_BLUE) {
    if ((int32_t)(now - ledIdentifyUntilMs) >= 0) {
      ledMode = ledModeBeforeIdentify;
      lastLedUpdateMs = 0;
      ledIdentifyPatternStep = 0;
      ws2812Set(0, 0, 0);
      return;
    }

    // Noticeable blue identify pattern: three quick bright flashes, one pause.
    const uint16_t identifyIntervalMs = 85;
    if (now - lastLedUpdateMs < identifyIntervalMs) {
      return;
    }

    lastLedUpdateMs = now;
    ledIdentifyPatternStep = (ledIdentifyPatternStep + 1U) % 8U;

    switch (ledIdentifyPatternStep) {
      case 1:
      case 3:
      case 5:
        ws2812Set(0, 0, 96);   // bright blue flash
        break;
      case 7:
        ws2812Set(0, 0, 18);   // low blue marker before pause
        break;
      default:
        ws2812Set(0, 0, 0);
        break;
    }
    return;
  }

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
