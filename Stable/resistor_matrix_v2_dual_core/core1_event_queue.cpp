/**
 * @file core1_event_queue.cpp
 * @brief Compact numeric Core 1 event queue.
 */
#include "core1_engine.h"

static Core1Event s_eventQueue[CORE1_EVENT_QUEUE_DEPTH];
static uint8_t s_eventHead = 0, s_eventTail = 0, s_eventCount = 0;
static spin_lock_t* s_eventLock = nullptr;
static volatile bool s_eventQueueInitialized = false;

bool initCore1EventQueue() {
  if (s_eventQueueInitialized) return true;
  if (s_eventLock == nullptr) s_eventLock = spin_lock_init(spin_lock_claim_unused(true));
  uint32_t irq = spin_lock_blocking(s_eventLock);
  if (!s_eventQueueInitialized) {
    s_eventHead = s_eventTail = s_eventCount = 0;
    s_eventQueueInitialized = true;
  }
  spin_unlock(s_eventLock, irq);
  return true;
}

bool core1EmitEvent(Core1EventCode code, FirmwareLogLevel level, uint8_t channelIndex,
                    uint16_t mask, uint16_t detail, uint32_t durationUs, uint32_t sequence) {
#if ERESISTOR_LOG_LEVEL < 0
  (void)code; (void)level; (void)channelIndex; (void)mask; (void)detail; (void)durationUs; (void)sequence;
  return true;
#else
  if (uint8_t(level) > uint8_t(ERESISTOR_LOG_LEVEL) || s_eventLock == nullptr) return s_eventLock != nullptr;
  Core1Event event{};
  event.timestampUs = micros();
  event.sequence = sequence;
  event.durationUs = durationUs;
  event.mask = mask;
  event.detail = detail;
  event.code = uint8_t(code);
  event.level = uint8_t(level);
  event.channelIndex = channelIndex;
  bool ok = false;
  uint32_t irq = spin_lock_blocking(s_eventLock);
  if (s_eventCount < CORE1_EVENT_QUEUE_DEPTH) {
    s_eventQueue[s_eventTail] = event;
    s_eventTail = uint8_t((s_eventTail + 1U) % CORE1_EVENT_QUEUE_DEPTH);
    s_eventCount++;
    ok = true;
  }
  spin_unlock(s_eventLock, irq);
  if (ok) core1EventCounter++; else core1EventDropCounter++;
  return ok;
#endif
}

bool popCore1Event(Core1Event& event) {
  if (s_eventLock == nullptr) return false;
  bool ok = false;
  uint32_t irq = spin_lock_blocking(s_eventLock);
  if (s_eventCount > 0U) {
    event = s_eventQueue[s_eventHead];
    s_eventHead = uint8_t((s_eventHead + 1U) % CORE1_EVENT_QUEUE_DEPTH);
    s_eventCount--;
    ok = true;
  }
  spin_unlock(s_eventLock, irq);
  return ok;
}
