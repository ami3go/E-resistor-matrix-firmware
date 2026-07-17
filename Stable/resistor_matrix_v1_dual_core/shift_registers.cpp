/**
 * @file shift_registers.cpp
 * @brief Core-1-owned physical shift-register and latch control for resistor outputs.
 */

#include "app.h"

// ============================================================
// Shift-register control
// ============================================================

void pulseClock() {
  digitalWrite(SR_CLOCK, HIGH);
  delayMicroseconds(SR_CLOCK_HALF_PERIOD_US);

  digitalWrite(SR_CLOCK, LOW);
  delayMicroseconds(SR_CLOCK_HALF_PERIOD_US);
}

void pulseLatch(uint8_t channelIndex) {
  uint8_t pin = SR_LATCH_PINS[channelIndex];

  digitalWrite(pin, HIGH);
  delayMicroseconds(SR_LATCH_PULSE_US);

  digitalWrite(pin, LOW);
  delayMicroseconds(SR_LATCH_PULSE_US);
}

void pulseShiftRegisterClear() {
  // 74HC595 SRCLR is active low. This clears the shift-register chain.
  // The output storage registers are then explicitly latched to zero.
  digitalWrite(SR_RESET, LOW);
  delayMicroseconds(20);
  digitalWrite(SR_RESET, HIGH);
  delayMicroseconds(20);
}

void shiftMaskBit0First(uint16_t mask) {
  for (uint8_t bit = 0; bit < BIT_COUNT; bit++) {
    bool value = (mask & (uint16_t(1) << bit)) != 0;

    digitalWrite(SR_DATA, value ? HIGH : LOW);
    delayMicroseconds(1);
    pulseClock();
  }

  digitalWrite(SR_DATA, LOW);
}

bool latchMaskToChannel(uint8_t channelIndex, uint16_t mask) {
  if (channelIndex >= CHANNEL_COUNT) {
    core1EmitEvent(CORE1_EVT_APPLY_FAILED, FW_LOG_ERROR, channelIndex, mask, CORE_RESP_INVALID_ARGUMENT, 0);
    return false;
  }

  shiftMaskBit0First(mask);
  pulseLatch(channelIndex);
  return true;
}

bool applyChannelMaskPhysical(uint8_t channelIndex, uint16_t newMask) {
  const uint32_t startUs = micros();

  if (!shiftRegistersReady) {
    core1EmitEvent(CORE1_EVT_APPLY_REJECTED, FW_LOG_ERROR, channelIndex, newMask, 1, 0);
    return false;
  }

  if (fatalSafeStateActive) {
    core1EmitEvent(CORE1_EVT_APPLY_REJECTED, FW_LOG_ERROR, channelIndex, newMask, 2, 0);
    return false;
  }

  if (channelIndex >= CHANNEL_COUNT) {
    core1EmitEvent(CORE1_EVT_APPLY_REJECTED, FW_LOG_ERROR, channelIndex, newMask, 3, 0);
    return false;
  }

  outputsKnownSafe = false;
  core1EmitEvent(CORE1_EVT_APPLY_BEGIN, FW_LOG_INFO, channelIndex, newMask, 0, 0);

  if (!latchMaskToChannel(channelIndex, 0x0000)) {
    core1EmitEvent(CORE1_EVT_APPLY_FAILED, FW_LOG_ERROR, channelIndex, newMask, 4, micros() - startUs);
    return false;
  }

  channelMask[channelIndex] = 0x0000;
  delay(BREAK_BEFORE_MAKE_MS);

  if (!latchMaskToChannel(channelIndex, newMask)) {
    core1EmitEvent(CORE1_EVT_APPLY_FAILED, FW_LOG_ERROR, channelIndex, newMask, 5, micros() - startUs);
    return false;
  }

  channelMask[channelIndex] = newMask;
  applyCounter[channelIndex]++;
  outputsKnownSafe = true;

  core1EmitEvent(CORE1_EVT_APPLY_DONE, FW_LOG_INFO, channelIndex, newMask, 0, micros() - startUs);
  return true;
}

bool applyAllMasksSafelyPhysical(const uint16_t masks[CHANNEL_COUNT], char* reason, size_t reasonLen) {
  if (reasonLen > 0U) {
    reason[0] = '\0';
  }

  if (masks == nullptr) {
    snprintf(reason, reasonLen, "missing mask array");
    return false;
  }

  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    if (!checkMaskSafety(ch, masks[ch], reason, reasonLen)) {
      core1EmitEvent(CORE1_EVT_PROFILE_FAILED, FW_LOG_ERROR, ch, masks[ch], CORE_RESP_REJECTED, 0);
      return false;
    }
  }

  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    if (!applyChannelMaskPhysical(ch, masks[ch])) {
      snprintf(reason, reasonLen, "failed to apply CH%u", unsigned(ch + 1));
      core1EmitEvent(CORE1_EVT_PROFILE_FAILED, FW_LOG_ERROR, ch, masks[ch], CORE_RESP_IO_ERROR, 0);
      // Core 1 must never queue an all-off request to itself.
      forceAllOffPhysical();
      return false;
    }
  }

  return true;
}

void setupShiftRegisters() {
  pinMode(SR_DATA, OUTPUT);
  digitalWrite(SR_DATA, LOW);

  pinMode(SR_CLOCK, OUTPUT);
  digitalWrite(SR_CLOCK, LOW);

  pinMode(SR_RESET, OUTPUT);
  digitalWrite(SR_RESET, HIGH);

  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    pinMode(SR_LATCH_PINS[ch], OUTPUT);
    digitalWrite(SR_LATCH_PINS[ch], LOW);
  }

  shiftRegistersReady = true;
  pulseShiftRegisterClear();
}

bool forceAllOffPhysical() {
  const uint32_t startUs = micros();
  core1EmitEvent(CORE1_EVT_ALL_OFF_BEGIN, FW_LOG_INFO, 0xFF, 0, 0, 0);

  if (!shiftRegistersReady) {
    outputsKnownSafe = false;
    core1EmitEvent(CORE1_EVT_ALL_OFF_FAILED, FW_LOG_ERROR, 0xFF, 0, 1, micros() - startUs);
    return false;
  }

  outputsKnownSafe = false;
  digitalWrite(SR_DATA, LOW);
  digitalWrite(SR_CLOCK, LOW);
  pulseShiftRegisterClear();

  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    if (!latchMaskToChannel(ch, 0x0000)) {
      core1EmitEvent(CORE1_EVT_ALL_OFF_FAILED, FW_LOG_ERROR, ch, 0, 2, micros() - startUs);
      return false;
    }
    channelMask[ch] = 0x0000;
    yield();
  }

  bool allZero = true;
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    if (channelMask[ch] != 0U) {
      allZero = false;
      break;
    }
  }

  outputsKnownSafe = allZero;
  core1EmitEvent(
    allZero ? CORE1_EVT_ALL_OFF_DONE : CORE1_EVT_ALL_OFF_FAILED,
    allZero ? FW_LOG_INFO : FW_LOG_ERROR,
    0xFF,
    0,
    allZero ? 0 : 3,
    micros() - startUs
  );
  return allZero;
}
