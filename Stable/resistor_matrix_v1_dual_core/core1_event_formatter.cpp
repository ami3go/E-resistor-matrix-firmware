/**
 * @file core1_event_formatter.cpp
 * @brief Core 0 formatting and USB serial output for compact Core 1 events.
 */

#include "app.h"

const char* core1EventCodeText(Core1EventCode code) {
  switch (code) {
    case CORE1_EVT_ENGINE_READY: return "ENGINE_READY";
    case CORE1_EVT_APPLY_BEGIN: return "APPLY_BEGIN";
    case CORE1_EVT_APPLY_DONE: return "APPLY_DONE";
    case CORE1_EVT_APPLY_REJECTED: return "APPLY_REJECTED";
    case CORE1_EVT_APPLY_FAILED: return "APPLY_FAILED";
    case CORE1_EVT_ALL_OFF_BEGIN: return "ALL_OFF_BEGIN";
    case CORE1_EVT_ALL_OFF_DONE: return "ALL_OFF_DONE";
    case CORE1_EVT_ALL_OFF_FAILED: return "ALL_OFF_FAILED";
    case CORE1_EVT_PROFILE_FAILED: return "PROFILE_FAILED";
    case CORE1_EVT_NONE:
    default: return "UNKNOWN";
  }
}

void drainCore1Events() {
  Core1Event event;
  bool wroteSerial = false;
  while (popCore1Event(event)) {
    char line[192];
    const unsigned channel = event.channelIndex < CHANNEL_COUNT
      ? unsigned(event.channelIndex + 1U)
      : 0U;

    snprintf(
      line,
      sizeof(line),
      "EVT ts_us=%lu core=1 seq=%lu level=%u code=%s ch=%u mask=0x%04X detail=%u duration_us=%lu",
      static_cast<unsigned long>(event.timestampUs),
      static_cast<unsigned long>(event.sequence),
      unsigned(event.level),
      core1EventCodeText(Core1EventCode(event.code)),
      channel,
      unsigned(event.mask),
      unsigned(event.detail),
      static_cast<unsigned long>(event.durationUs)
    );

    Serial.println(line);
    wroteSerial = true;

    if (event.level == FW_LOG_ERROR || event.code == CORE1_EVT_ENGINE_READY) {
      appendLogEvent(line);
    }
  }
  if (wroteSerial) {
    Serial.flush();
  }
}
