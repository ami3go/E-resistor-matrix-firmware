/**
 * @file status_led.cpp
 * @brief WS2812 heartbeat and fault/status indication logic.
 *
 * Visibility-focused implementation:
 * - Higher global brightness than the old very-dim setting.
 * - Each mode has a low/base color instead of spending half the time fully OFF.
 * - Fault and activity states use short, bright flashes.
 * - Public API is kept compatible: ws2812Set(), heartbeatBegin(), setLedMode(), updateHeartbeat().
 */

#include "app.h"

// Forward declaration so private helper functions below can call ws2812Set()
// even if app.h does not declare it.
void ws2812Set(uint8_t r, uint8_t g, uint8_t b);

// 04_status_led.ino
// Split from rp2040_w5500_resistor_matrix_v1_safety_logic.ino.
// Keep all files in the same Arduino sketch folder.

// ============================================================
// WS2812 heartbeat
// ============================================================

namespace {

// WS2812 brightness is global scaling. 48 is still modest, but much easier
// to see through many enclosures than the previous value of 24.
constexpr uint8_t LED_GLOBAL_BRIGHTNESS = 48;

// Low/base levels keep the LED visible even during the "off" phase.
// Values are intentionally conservative to avoid heating/power noise.
constexpr uint8_t BOOT_BASE_B = 4;
constexpr uint8_t OK_BASE_G = 4;
constexpr uint8_t ACTIVITY_BASE_R = 3;
constexpr uint8_t ACTIVITY_BASE_G = 2;
constexpr uint8_t FAULT_BASE_R = 5;

// Bright flash levels. Keep below 255 because a single onboard WS2812 can be
// painfully bright and may inject noise into sensitive analog measurements.
constexpr uint8_t BOOT_FLASH_B = 40;
constexpr uint8_t OK_FLASH_G = 36;
constexpr uint8_t ACTIVITY_FLASH_R = 52;
constexpr uint8_t ACTIVITY_FLASH_G = 28;
constexpr uint8_t FAULT_FLASH_R = 80;

/**
 * @brief Return the heartbeat update interval for the active LED mode.
 * @param mode Current LED mode.
 * @return Update interval in milliseconds.
 */
uint16_t ledIntervalMs(LedMode mode) {
  switch (mode) {
    case LED_ACTIVITY:
      return 90;    // Fast amber flicker during traffic/activity.
    case LED_FAULT:
      return 180;   // Clearly urgent red flash.
    case LED_BOOT:
      return 300;   // Medium blue boot pulse.
    case LED_OK:
    default:
      return 850;   // Slow green heartbeat when healthy.
  }
}

/**
 * @brief Set the low/base visible color for a mode.
 *
 * This replaces the previous fully-off phase. Fully dark LEDs can be hard to
 * distinguish from firmware hang, power loss, or bad wiring.
 */
void setBaseColorForMode(LedMode mode) {
  switch (mode) {
    case LED_BOOT:
      ws2812Set(0, 0, BOOT_BASE_B);
      break;

    case LED_OK:
      ws2812Set(0, OK_BASE_G, 0);
      break;

    case LED_ACTIVITY:
      ws2812Set(ACTIVITY_BASE_R, ACTIVITY_BASE_G, 0);
      break;

    case LED_FAULT:
    default:
      ws2812Set(FAULT_BASE_R, 0, 0);
      break;
  }
}

/**
 * @brief Set the high/flash color for a mode.
 */
void setFlashColorForMode(LedMode mode) {
  switch (mode) {
    case LED_BOOT:
      ws2812Set(0, 0, BOOT_FLASH_B);
      break;

    case LED_OK:
      ws2812Set(0, OK_FLASH_G, 0);
      break;

    case LED_ACTIVITY:
      ws2812Set(ACTIVITY_FLASH_R, ACTIVITY_FLASH_G, 0);
      break;

    case LED_FAULT:
    default:
      ws2812Set(FAULT_FLASH_R, 0, 0);
      break;
  }
}

}  // namespace

/**
 * @brief Set the WS2812 pixel color immediately.
 * @param r Red intensity, 0..255 before global brightness scaling.
 * @param g Green intensity, 0..255 before global brightness scaling.
 * @param b Blue intensity, 0..255 before global brightness scaling.
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
  heartbeatPixel.setBrightness(LED_GLOBAL_BRIGHTNESS);
  setBaseColorForMode(LED_BOOT);
}

/**
 * @brief Set LED status mode and force the next update to happen immediately.
 * @param mode New LED mode.
 */
void setLedMode(LedMode mode) {
  ledMode = mode;
  ledPhase = false;
  lastLedUpdateMs = 0;
  setBaseColorForMode(ledMode);
}

/**
 * @brief Update the WS2812 indicator according to the current LED mode.
 *
 * Call this frequently from loop(). The function is non-blocking and updates
 * the LED only when the mode-specific interval has elapsed.
 */
void updateHeartbeat() {
  const uint32_t now = millis();
  const uint16_t intervalMs = ledIntervalMs(ledMode);

  if ((uint32_t)(now - lastLedUpdateMs) < intervalMs) {
    return;
  }

  lastLedUpdateMs = now;
  ledPhase = !ledPhase;

  if (ledPhase) {
    setFlashColorForMode(ledMode);
  } else {
    setBaseColorForMode(ledMode);
  }
}
