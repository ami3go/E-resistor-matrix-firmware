/**
 * @file shift_registers.cpp
 * @brief Core-1-owned physical shift-register and latch control for resistor outputs.
 */

#include "app.h"

// 06_shift_registers.ino
// Split from rp2040_w5500_resistor_matrix_v1_safety_logic.ino.
// Keep all files in the same Arduino sketch folder.

// ============================================================
// Shift-register control
// ============================================================

/**
 * @brief Generate one shift-register clock pulse using the configured timing.
 */
void pulseClock() {
  digitalWrite(SR_CLOCK, HIGH);
  delayMicroseconds(SR_CLOCK_HALF_PERIOD_US);

  digitalWrite(SR_CLOCK, LOW);
  delayMicroseconds(SR_CLOCK_HALF_PERIOD_US);
}

/**
 * @brief Pulse exactly one channel latch pin.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 */
void pulseLatch(uint8_t channelIndex) {
  uint8_t pin = SR_LATCH_PINS[channelIndex];

  digitalWrite(pin, HIGH);
  delayMicroseconds(SR_LATCH_PULSE_US);

  digitalWrite(pin, LOW);
  delayMicroseconds(SR_LATCH_PULSE_US);
}

/**
 * @brief Pulse SRCLR low to clear the cascaded 74HC595 outputs.
 */
void pulseShiftRegisterClear() {
  // 74HC595 SRCLR is active low. This clears the shift-register chain.
  // Outputs are then explicitly latched to zero by forceAllOff().
  digitalWrite(SR_RESET, LOW);
  delayMicroseconds(20);
  digitalWrite(SR_RESET, HIGH);
  delayMicroseconds(20);
}

/**
 * @brief Shift a 16-bit mask to the register chain with bit 0 transmitted first.
 * @param mask 16-bit resistance switch mask.
 */
void shiftMaskBit0First(uint16_t mask) {
  for (uint8_t bit = 0; bit < BIT_COUNT; bit++) {
    bool value = (mask & (uint16_t(1) << bit)) != 0;

    digitalWrite(SR_DATA, value ? HIGH : LOW);
    delayMicroseconds(1);

    pulseClock();
  }

  digitalWrite(SR_DATA, LOW);
}

/**
 * @brief Shift and latch a mask into exactly one channel output register.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @param mask 16-bit resistance switch mask.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool latchMaskToChannel(uint8_t channelIndex, uint16_t mask) {
  if (channelIndex >= CHANNEL_COUNT) {
    Serial.println("latchMaskToChannel: bad channel");
    Serial.flush();
    return false;
  }

  Serial.print("  latchMaskToChannel CH");
  Serial.print(channelIndex + 1);
  Serial.print(" mask=");
  Serial.println(hex16(mask));
  Serial.flush();

  shiftMaskBit0First(mask);
  pulseLatch(channelIndex);

  Serial.println("  latchMaskToChannel done");
  Serial.flush();

  return true;
}

/**
 * @brief Apply one channel mask with break-before-make sequencing on Core 1.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @param newMask 16-bit resistance switch mask.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool applyChannelMaskPhysical(uint8_t channelIndex, uint16_t newMask) {
  Serial.println("applyChannelMask: start");
  Serial.flush();

  if (!shiftRegistersReady) {
    Serial.println("applyChannelMask: shift registers are not ready");
    Serial.flush();
    setLastError("-900,\"shift registers not ready\"");
    return false;
  }

  if (fatalSafeStateActive) {
    Serial.println("applyChannelMask: rejected because fatal safe-state is active");
    Serial.flush();
    setLastError("-300,\"safe state active\"");
    return false;
  }

  if (channelIndex >= CHANNEL_COUNT) {
    Serial.println("applyChannelMask: bad channel");
    Serial.flush();
    return false;
  }

  outputsKnownSafe = false;

  Serial.print("applyChannelMask: CH");
  Serial.print(channelIndex + 1);
  Serial.print(" newMask=");
  Serial.println(hex16(newMask));
  Serial.flush();

  Serial.println("applyChannelMask: break-before-make OFF");
  Serial.flush();

  if (!latchMaskToChannel(channelIndex, 0x0000)) {
    Serial.println("applyChannelMask: OFF latch failed");
    Serial.flush();
    return false;
  }

  channelMask[channelIndex] = 0x0000;

  Serial.println("applyChannelMask: wait BBM");
  Serial.flush();

  delay(BREAK_BEFORE_MAKE_MS);

  Serial.println("applyChannelMask: latch new mask");
  Serial.flush();

  if (!latchMaskToChannel(channelIndex, newMask)) {
    Serial.println("applyChannelMask: new latch failed");
    Serial.flush();
    return false;
  }

  channelMask[channelIndex] = newMask;
  applyCounter[channelIndex]++;
  outputsKnownSafe = true;

  Serial.println("applyChannelMask: done");
  Serial.flush();

  return true;
}

/**
 * @brief Validate and apply an eight-channel mask set using physical Core 1 switching.
 * @param masks 16-bit resistance switch mask.
 * @param reason Output buffer for a human-readable diagnostic message.
 * @param reasonLen Output buffer for a human-readable diagnostic message.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool applyAllMasksSafelyPhysical(const uint16_t masks[CHANNEL_COUNT], char* reason, size_t reasonLen) {
  if (reasonLen > 0) {
    reason[0] = '\0';
  }

  if (masks == nullptr) {
    snprintf(reason, reasonLen, "missing mask array");
    return false;
  }

  // Validate the complete requested state before changing any output.
  // This prevents half-applied multi-channel commands caused by a late
  // safety rejection on channel N.
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    if (!checkMaskSafety(ch, masks[ch], reason, reasonLen)) {
      return false;
    }
  }

  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    if (!applyChannelMaskPhysical(ch, masks[ch])) {
      snprintf(reason, reasonLen, "failed to apply CH%u", unsigned(ch + 1));
      safeState(reason);
      return false;
    }
  }

  return true;
}

/**
 * @brief Configure shift-register GPIO pins, pulse SRCLR, and force known-safe outputs.
 */
void setupShiftRegisters() {
  Serial.println("STEP 1: setup shift-register pin modes");
  Serial.flush();

  pinMode(SR_DATA, OUTPUT);
  digitalWrite(SR_DATA, LOW);
  Serial.println("  SR_DATA GP11 OK");
  Serial.flush();

  pinMode(SR_CLOCK, OUTPUT);
  digitalWrite(SR_CLOCK, LOW);
  Serial.println("  SR_CLOCK GP12 OK");
  Serial.flush();

  pinMode(SR_RESET, OUTPUT);
  digitalWrite(SR_RESET, HIGH);
  Serial.println("  SR_RESET GP15 OK");
  Serial.flush();

  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    pinMode(SR_LATCH_PINS[ch], OUTPUT);
    digitalWrite(SR_LATCH_PINS[ch], LOW);

    Serial.print("  LATCH CH");
    Serial.print(ch + 1);
    Serial.print(" GP");
    Serial.print(SR_LATCH_PINS[ch]);
    Serial.println(" OK");
    Serial.flush();
  }

  shiftRegistersReady = true;

  Serial.println("  SRCLR low pulse");
  Serial.flush();
  pulseShiftRegisterClear();
}

/**
 * @brief Physically latch 0x0000 into all eight output channels.
 */
void forceAllOffPhysical() {
  Serial.println("Shift registers: all OFF start");
  Serial.flush();

  if (!shiftRegistersReady) {
    outputsKnownSafe = false;
    setLastError("-900,\"shift registers not ready\"");
    Serial.println("Shift registers: all OFF refused, GPIOs not ready");
    Serial.flush();
    return;
  }

  outputsKnownSafe = false;
  setLedMode(LED_ACTIVITY);

  digitalWrite(SR_DATA, LOW);
  digitalWrite(SR_CLOCK, LOW);

  // SRCLR is active-low. Pulse it first, then latch zero to every channel so
  // the 74HC595 output registers are explicitly driven to the safe OFF state.
  pulseShiftRegisterClear();

  Serial.println("  basic SR pins set and SRCLR pulsed");
  Serial.flush();

  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    Serial.print("  latch zero to CH");
    Serial.println(ch + 1);
    Serial.flush();

    shiftMaskBit0First(0x0000);
    pulseLatch(ch);

    channelMask[ch] = 0x0000;
    yield();
  }

  outputsKnownSafe = true;
  setStatus("All channels OFF");
  if (!fatalSafeStateActive) {
    setLedMode(LED_OK);
  }

  Serial.println("Shift registers: all OFF done");
  Serial.flush();
}

