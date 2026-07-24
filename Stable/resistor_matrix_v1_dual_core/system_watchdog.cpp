/**
 * @file system_watchdog.cpp
 * @brief Hardware watchdog gated on two-core liveness.
 *
 * Before this module the RP2040 watchdog was unused: the /watchdog page simply
 * reported "not enabled". That left one unrecovered failure mode on a device
 * that drives power resistors:
 *
 *   - Core 0 wedges  -> Core 1's CORE0_FAILSAFE_TIMEOUT_MS failsafe clears the
 *                       outputs after 10 s. Covered.
 *   - Core 1 wedges  -> nothing recovers. Outputs stay latched indefinitely.
 *
 * The feed is therefore deliberately conditional. Core 0 only services the
 * watchdog while Core 1's heartbeat is fresh, so a wedged Core 1 starves the
 * timer and forces a reset. A watchdog fed unconditionally from loop() would
 * have re-armed forever and left exactly the gap it was added to close.
 *
 * @warning Reset does not instantly de-energise the outputs. The 74HC595
 * outputs hold their last latched state while the RP2040 reboots and its GPIOs
 * are high-impedance; they are cleared a few milliseconds later when
 * core1PhysicalInitialize() drives SR_RESET and pulses the latches. Closing
 * that window needs hardware (drive OE from a pin with a pull-up to disabled,
 * or an RC reset on MR). Tracked as a hardware item; the firmware cannot fix it.
 */

#include "app.h"
#include <hardware/watchdog.h>
#include <hardware/structs/watchdog.h>

namespace {

// RP2040 watchdog counts in microseconds in a 24-bit register, so the usable
// ceiling is ~8.38 s. 4 s leaves margin above the worst observed Core 0 loop
// while still bounding the outputs-stuck-on window.
constexpr uint32_t WATCHDOG_TIMEOUT_MS = 4000UL;

// Core 1 refreshes core1HeartbeatMs every pass of core1ProcessEngineOnce(),
// nominally sub-millisecond. 2 s is far outside normal jitter, so starvation
// means genuinely stalled, not merely busy.
constexpr uint32_t CORE1_LIVENESS_MAX_AGE_MS = 2000UL;

bool s_enabled = false;
bool s_rebootedByWatchdog = false;
bool s_suspended = false;
uint32_t s_lastFeedMs = 0;
uint32_t s_starvedCount = 0;
uint32_t s_lastCore1AgeMs = 0;

}  // namespace

void watchdogCaptureBootReason() {
  // Must be read before the watchdog is re-armed, otherwise the flag reflects
  // this boot's configuration rather than what caused the previous reset.
  s_rebootedByWatchdog = watchdog_caused_reboot();
}

void watchdogBegin() {
  if (s_enabled) return;
  // pause_on_debug = true so a halted debugger does not spuriously reset.
  watchdog_enable(WATCHDOG_TIMEOUT_MS, true);
  s_enabled = true;
  s_suspended = false;
  s_lastFeedMs = millis();
  watchdog_update();
  appendLogEvent("Watchdog enabled, gated on Core 1 liveness");
}

void watchdogService() {
  if (!s_enabled) return;

  if (s_suspended) {
    // Deliberate blocking maintenance (OTA). Keep the timer alive; the caller
    // is responsible for bounding how long this lasts.
    watchdog_update();
    s_lastFeedMs = millis();
    return;
  }

  const uint32_t nowMs = millis();
  const uint32_t core1AgeMs = nowMs - uint32_t(core1HeartbeatMs);
  s_lastCore1AgeMs = core1AgeMs;

  if (core1AgeMs > CORE1_LIVENESS_MAX_AGE_MS) {
    // Starve the watchdog on purpose. This is the recovery path for a wedged
    // Core 1 and must not be "fixed" by feeding unconditionally.
    if (s_starvedCount == 0U) {
      char message[128];
      snprintf(message, sizeof(message),
               "Watchdog starved: Core 1 heartbeat stale %lu ms; reset pending",
               static_cast<unsigned long>(core1AgeMs));
      appendLogEvent(message);
    }
    s_starvedCount++;
    return;
  }

  watchdog_update();
  s_lastFeedMs = nowMs;
}

void watchdogSuspend(const char* reason) {
  if (!s_enabled || s_suspended) return;
  // Clearing ENABLE stops the counter outright. Used for operations that block
  // longer than the timeout and cannot be instrumented to feed (Update.writeStream).
  hw_clear_bits(&watchdog_hw->ctrl, WATCHDOG_CTRL_ENABLE_BITS);
  s_suspended = true;
  char message[128];
  snprintf(message, sizeof(message), "Watchdog suspended: %s", reason ? reason : "unspecified");
  appendLogEvent(message);
}

void watchdogResume() {
  if (!s_enabled || !s_suspended) return;
  watchdog_enable(WATCHDOG_TIMEOUT_MS, true);
  s_suspended = false;
  s_lastFeedMs = millis();
  watchdog_update();
  appendLogEvent("Watchdog resumed");
}

bool watchdogEnabled() { return s_enabled; }
bool watchdogSuspended() { return s_suspended; }
bool watchdogCausedLastReboot() { return s_rebootedByWatchdog; }
uint32_t watchdogTimeoutMs() { return WATCHDOG_TIMEOUT_MS; }
uint32_t watchdogStarvedCount() { return s_starvedCount; }
uint32_t watchdogLastCore1AgeMs() { return s_lastCore1AgeMs; }

uint32_t watchdogLastFeedAgeMs() {
  return s_enabled ? (millis() - s_lastFeedMs) : 0U;
}

const char* watchdogStateText() {
  if (!s_enabled) return "not enabled";
  if (s_suspended) return "suspended for maintenance";
  return "armed";
}
