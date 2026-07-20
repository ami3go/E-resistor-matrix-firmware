/**
 * @file shift_registers.cpp
 * @brief Core-1-owned physical shift-register engine.
 */
#include "core1_engine.h"

static bool s_shiftRegistersReady = false;
static bool s_outputsSafe = false;
static uint16_t s_masks[CHANNEL_COUNT] = {};
static uint32_t s_applyCounters[CHANNEL_COUNT] = {};

static void pulseClockPhysical() {
  digitalWrite(SR_CLOCK, HIGH);
  delayMicroseconds(SR_CLOCK_HALF_PERIOD_US);
  digitalWrite(SR_CLOCK, LOW);
  delayMicroseconds(SR_CLOCK_HALF_PERIOD_US);
}

static void pulseLatchPhysical(uint8_t channelIndex) {
  const uint8_t pin = SR_LATCH_PINS[channelIndex];
  digitalWrite(pin, HIGH);
  delayMicroseconds(SR_LATCH_PULSE_US);
  digitalWrite(pin, LOW);
  delayMicroseconds(SR_LATCH_PULSE_US);
}

static void pulseClearPhysical() {
  digitalWrite(SR_RESET, LOW);
  delayMicroseconds(20);
  digitalWrite(SR_RESET, HIGH);
  delayMicroseconds(20);
}

static void shiftMaskPhysical(uint16_t mask) {
  for (uint8_t bit = 0; bit < BIT_COUNT; ++bit) {
    digitalWrite(SR_DATA, (mask & (uint16_t(1U) << bit)) ? HIGH : LOW);
    delayMicroseconds(1);
    pulseClockPhysical();
  }
  digitalWrite(SR_DATA, LOW);
}

static bool latchMaskPhysical(uint8_t channelIndex, uint16_t mask) {
  if (channelIndex >= CHANNEL_COUNT || !s_shiftRegistersReady) return false;
  shiftMaskPhysical(mask);
  pulseLatchPhysical(channelIndex);
  return true;
}

bool core1PhysicalInitialize() {
  pinMode(SR_DATA, OUTPUT);
  digitalWrite(SR_DATA, LOW);
  pinMode(SR_CLOCK, OUTPUT);
  digitalWrite(SR_CLOCK, LOW);
  pinMode(SR_RESET, OUTPUT);
  digitalWrite(SR_RESET, HIGH);
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
    pinMode(SR_LATCH_PINS[ch], OUTPUT);
    digitalWrite(SR_LATCH_PINS[ch], LOW);
  }
  s_shiftRegistersReady = true;
  pulseClearPhysical();
  return true;
}

bool core1ApplyChannelMaskPhysical(uint8_t channelIndex, uint16_t newMask, uint32_t sequence) {
  const uint32_t startUs = micros();
  if (!s_shiftRegistersReady || channelIndex >= CHANNEL_COUNT) return false;
  s_outputsSafe = false;
  core1EmitEvent(CORE1_EVT_APPLY_BEGIN, FW_LOG_DEBUG, channelIndex, newMask, 0, 0, sequence);
  if (!latchMaskPhysical(channelIndex, 0U)) return false;
  s_masks[channelIndex] = 0U;
  delay(BREAK_BEFORE_MAKE_MS);
  if (!latchMaskPhysical(channelIndex, newMask)) return false;
  s_masks[channelIndex] = newMask;
  s_applyCounters[channelIndex]++;
  s_outputsSafe = true;
  core1EmitEvent(CORE1_EVT_APPLY_DONE, FW_LOG_INFO, channelIndex, newMask, 0, micros() - startUs, sequence);
  return true;
}

bool core1ApplyAllMasksPhysical(const uint16_t masks[CHANNEL_COUNT], uint32_t sequence) {
  if (masks == nullptr) return false;
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
    if (!core1ApplyChannelMaskPhysical(ch, masks[ch], sequence)) {
      core1ForceAllOffPhysical(sequence, CORE_DETAIL_PHYSICAL_APPLY_FAILED);
      return false;
    }
  }
  return true;
}

bool core1ForceAllOffPhysical(uint32_t sequence, uint16_t detail) {
  const uint32_t startUs = micros();
  if (!s_shiftRegistersReady) return false;
  s_outputsSafe = false;
  core1EmitEvent(CORE1_EVT_ALL_OFF_BEGIN, FW_LOG_INFO, 0xFF, 0, detail, 0, sequence);
  digitalWrite(SR_DATA, LOW);
  digitalWrite(SR_CLOCK, LOW);
  pulseClearPhysical();
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
    if (!latchMaskPhysical(ch, 0U)) {
      core1EmitEvent(CORE1_EVT_ALL_OFF_FAILED, FW_LOG_ERROR, ch, 0, detail, micros() - startUs, sequence);
      return false;
    }
    s_masks[ch] = 0U;
  }
  s_outputsSafe = true;
  core1EmitEvent(CORE1_EVT_ALL_OFF_DONE, FW_LOG_INFO, 0xFF, 0, detail, micros() - startUs, sequence);
  return true;
}

const uint16_t* core1PhysicalMasks() { return s_masks; }
const uint32_t* core1PhysicalApplyCounters() { return s_applyCounters; }
bool core1PhysicalOutputsSafe() { return s_outputsSafe; }
bool core1PhysicalReady() { return s_shiftRegistersReady; }
