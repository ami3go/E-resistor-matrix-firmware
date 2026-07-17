/**
 * @file core1_event_queue.cpp
 * @brief Compact Core 1 diagnostic event queue and Core 0 formatter.
 */

#include "app.h"

static Core1Event eventQueue[CORE1_EVENT_QUEUE_DEPTH];
static uint8_t eventHead = 0;
static uint8_t eventTail = 0;
static uint8_t eventCount = 0;
static spin_lock_t* eventSpinLock = nullptr;

void initCore1EventQueue() {
  if (eventSpinLock == nullptr) {
    uint lockNum = spin_lock_claim_unused(true);
    eventSpinLock = spin_lock_init(lockNum);
  }

  uint32_t irqState = spin_lock_blocking(eventSpinLock);
  eventHead = 0;
  eventTail = 0;
  eventCount = 0;
  spin_unlock(eventSpinLock, irqState);
}

bool core1EmitEvent(
  Core1EventCode code,
  FirmwareLogLevel level,
  uint8_t channelIndex,
  uint16_t mask,
  uint16_t detail,
  uint32_t durationUs
) {
#if ERESISTOR_LOG_LEVEL < 0
  (void)code;
  (void)level;
  (void)channelIndex;
  (void)mask;
  (void)detail;
  (void)durationUs;
  return true;
#else
  if (uint8_t(level) > uint8_t(ERESISTOR_LOG_LEVEL)) {
    return true;
  }

  if (eventSpinLock == nullptr) {
    core1EventDropCounter++;
    return false;
  }

  Core1Event event{};
  event.timestampUs = micros();
  event.sequence = core1CommandCounter;
  event.durationUs = durationUs;
  event.mask = mask;
  event.detail = detail;
  event.code = uint8_t(code);
  event.level = uint8_t(level);
  event.channelIndex = channelIndex;

  bool ok = false;
  uint32_t irqState = spin_lock_blocking(eventSpinLock);
  if (eventCount < CORE1_EVENT_QUEUE_DEPTH) {
    eventQueue[eventTail] = event;
    eventTail = uint8_t((eventTail + 1U) % CORE1_EVENT_QUEUE_DEPTH);
    eventCount++;
    ok = true;
  }
  spin_unlock(eventSpinLock, irqState);

  if (ok) {
    core1EventCounter++;
  } else {
    core1EventDropCounter++;
  }
  return ok;
#endif
}

bool popCore1Event(Core1Event& event) {
  if (eventSpinLock == nullptr) {
    return false;
  }

  bool ok = false;
  uint32_t irqState = spin_lock_blocking(eventSpinLock);
  if (eventCount > 0U) {
    event = eventQueue[eventHead];
    eventHead = uint8_t((eventHead + 1U) % CORE1_EVENT_QUEUE_DEPTH);
    eventCount--;
    ok = true;
  }
  spin_unlock(eventSpinLock, irqState);
  return ok;
}

