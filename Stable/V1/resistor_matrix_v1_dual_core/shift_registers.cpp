/**
 * @file shift_registers.cpp
 * @brief Core-1-owned physical shift-register and latch control for resistor outputs.
 */

#include "app.h"

// ============================================================
// Shift-register control
//
// Stability notes:
//   * Core 1 is the only core allowed to touch SR_DATA, SR_CLOCK, SR_RESET,
//     and the channel latch pins.
//   * A complete shift+latch transaction is protected from local Core-1
//     interrupts. Core 0 keeps running Ethernet/HTTP/SCPI independently.
//   * Runtime Serial.flush() calls are intentionally kept out of the physical
//     timing path; USB serial can block and make switching appear unstable.
// ============================================================

/**
 * @brief Optional low-rate shift-register diagnostic print.
 * @param text Message to print when SR_VERBOSE_LOG is enabled.
 */
static void srLog(const char* text) {
  if (SR_VERBOSE_LOG && text != nullptr) {
    Serial.println(text);
  }
}

/**
 * @brief Configure one RP2040 GPIO for slow, non-ringing shift-register edges.
 *
 * Bit 0 is the first bit shifted through the complete 16-bit chain. A single
 * false SH_CP edge after the transfer can push that bit out of the chain, so
 * the firmware deliberately uses slow/low-drive GPIO edges where the RP2040
 * core supports it. External 33..100 ohm series resistors are still recommended
 * on SR_CLOCK, SR_DATA, and long latch traces.
 */
static void configureStableSrOutput(uint8_t pin) {
  pinMode(pin, OUTPUT);

#if defined(ARDUINO_ARCH_RP2040)
  gpio_set_drive_strength(pin, GPIO_DRIVE_STRENGTH_2MA);
  gpio_set_slew_rate(pin, GPIO_SLEW_RATE_SLOW);
#endif
}

/**
 * @brief Keep every channel latch input low while the shared serial chain is shifting.
 */
static void setAllLatchesLow() {
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    digitalWrite(SR_LATCH_PINS[ch], LOW);
  }
}

/**
 * @brief Drive the serial bus to an idle-safe level.
 */
static void srBusIdle() {
  digitalWrite(SR_CLOCK, LOW);
  digitalWrite(SR_DATA, LOW);
}

/**
 * @brief Generate one shift-register clock pulse using the configured timing.
 */
void pulseClock() {
  // 74HC595 shifts on the rising edge of SH_CP. The caller guarantees that
  // SR_CLOCK is already LOW before the first pulse, and every call ends LOW.
  // Do not insert an extra pre-low delay here; it only stretches the transfer.
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
  if (channelIndex >= CHANNEL_COUNT) {
    return;
  }

  const uint8_t pin = SR_LATCH_PINS[channelIndex];

  // 74HC595 ST_CP samples the shift-register state on the rising edge. The
  // caller has already forced all latch lines LOW and waited the setup time,
  // so this function generates only the latch pulse itself.
  digitalWrite(pin, HIGH);
  delayMicroseconds(SR_LATCH_PULSE_US);

  digitalWrite(pin, LOW);
  delayMicroseconds(SR_POST_LATCH_SETTLE_US);
}

/**
 * @brief Pulse SRCLR low to clear the cascaded 74HC595 shift registers.
 */
void pulseShiftRegisterClear() {
  // SRCLR clears only the internal shift register. The output register changes
  // only after a latch pulse, so forceAllOffPhysical() still latches zero into
  // every channel after this pulse.
  digitalWrite(SR_RESET, HIGH);
  delayMicroseconds(SR_CLEAR_PULSE_US);

  digitalWrite(SR_RESET, LOW);
  delayMicroseconds(SR_CLEAR_PULSE_US);

  digitalWrite(SR_RESET, HIGH);
  delayMicroseconds(SR_CLEAR_PULSE_US);
}

/**
 * @brief Shift a 16-bit mask to the register chain with bit 0 transmitted first.
 * @param mask 16-bit resistance switch mask.
 */
void shiftMaskBit0First(uint16_t mask) {
  // Hardware mapping requirement:
  //   mask bit 0  -> Q16, lowest resistance branch
  //   mask bit 15 -> Q1, highest resistance branch
  // Therefore bit 0 is transmitted first and bit 15 last by default.
  // SR_SHIFT_BIT0_FIRST is left as a compile-time diagnostic switch because a
  // reversed 74HC595 cascade will make the web state look correct while the
  // physical branch appears not to switch.
  digitalWrite(SR_CLOCK, LOW);
  delayMicroseconds(SR_CLOCK_HALF_PERIOD_US);

  if (SR_SHIFT_BIT0_FIRST) {
    for (uint8_t bit = 0; bit < BIT_COUNT; bit++) {
      const bool value = (mask & (uint16_t(1) << bit)) != 0;
      digitalWrite(SR_DATA, value ? HIGH : LOW);
      delayMicroseconds(SR_DATA_SETUP_US);
      pulseClock();
    }
  } else {
    for (int8_t bit = BIT_COUNT - 1; bit >= 0; bit--) {
      const bool value = (mask & (uint16_t(1) << uint8_t(bit))) != 0;
      digitalWrite(SR_DATA, value ? HIGH : LOW);
      delayMicroseconds(SR_DATA_SETUP_US);
      pulseClock();
    }
  }

  digitalWrite(SR_DATA, LOW);
  delayMicroseconds(SR_PRE_LATCH_SETTLE_US);
}

/**
 * @brief Shift and latch a mask into exactly one channel output register.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @param mask 16-bit resistance switch mask.
 * @return true when the operation succeeded.
 */
bool latchMaskToChannel(uint8_t channelIndex, uint16_t mask) {
  if (channelIndex >= CHANNEL_COUNT) {
    setLastError("-101,\"invalid channel\"");
    return false;
  }

  const uint8_t latchPin = SR_LATCH_PINS[channelIndex];

  // Keep all timing-sensitive SR pins controlled by one uninterrupted Core-1
  // transaction. This prevents timer/USB callbacks on Core 1 from stretching a
  // clock or latch edge in the middle of a hardware update.
  noInterrupts();

  // All ST_CP inputs must be low while SH_CP is active. This avoids accidental
  // partial latching on channels with long or noisy latch traces.
  setAllLatchesLow();
  srBusIdle();
  delayMicroseconds(SR_LATCH_SETUP_US);

  shiftMaskBit0First(mask);
  digitalWrite(SR_CLOCK, LOW);
  delayMicroseconds(SR_PRE_LATCH_SETTLE_US);
  pulseLatch(channelIndex);

  setAllLatchesLow();
  srBusIdle();

  interrupts();

  if (SR_VERBOSE_LOG) {
    Serial.print("SR CH");
    Serial.print(channelIndex + 1);
    Serial.print(" <= ");
    Serial.println(hex16(mask));
  }

  return true;
}

/**
 * @brief Apply one channel mask with break-before-make sequencing on Core 1.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @param newMask 16-bit resistance switch mask.
 * @return true when the operation succeeded.
 */
bool applyChannelMaskPhysical(uint8_t channelIndex, uint16_t newMask) {
  if (!shiftRegistersReady) {
    setLastError("-900,\"shift registers not ready\"");
    return false;
  }

  if (fatalSafeStateActive) {
    setLastError("-300,\"safe state active\"");
    return false;
  }

  if (channelIndex >= CHANNEL_COUNT) {
    setLastError("-101,\"invalid channel\"");
    return false;
  }

  outputsKnownSafe = false;

  // Break-before-make: first switch the selected channel fully OFF, then wait,
  // then latch the requested new state. This avoids momentary parallel shorts
  // caused by overlapping MOSFET states during resistance changes.
  if (!latchMaskToChannel(channelIndex, 0x0000)) {
    return false;
  }
  channelMask[channelIndex] = 0x0000;

  delay(BREAK_BEFORE_MAKE_MS);

  for (uint8_t repeat = 0; repeat < SR_FINAL_WRITE_REPEATS; repeat++) {
    if (!latchMaskToChannel(channelIndex, newMask)) {
      return false;
    }
    if (repeat + 1U < SR_FINAL_WRITE_REPEATS) {
      delayMicroseconds(SR_POST_LATCH_SETTLE_US);
    }
  }

  channelMask[channelIndex] = newMask;
  applyCounter[channelIndex]++;
  outputsKnownSafe = true;

  return true;
}

/**
 * @brief Validate and apply an eight-channel mask set using physical Core 1 switching.
 * @param masks 16-bit resistance switch mask.
 * @param reason Output buffer for a human-readable diagnostic message.
 * @param reasonLen Output buffer for a human-readable diagnostic message.
 * @return true when the operation succeeded.
 */
bool applyAllMasksSafelyPhysical(const uint16_t masks[CHANNEL_COUNT], char* reason, size_t reasonLen) {
  if (reasonLen > 0U) {
    reason[0] = '\0';
  }

  if (masks == nullptr) {
    snprintf(reason, reasonLen, "missing mask array");
    return false;
  }

  // Validate the complete requested state before changing any output. This
  // prevents half-applied multi-channel commands caused by a late safety
  // rejection on channel N.
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    if (!checkMaskSafety(ch, masks[ch], reason, reasonLen)) {
      return false;
    }
  }

  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    if (!applyChannelMaskPhysical(ch, masks[ch])) {
      snprintf(reason, reasonLen, "failed to apply CH%u", unsigned(ch + 1));

      // This function already runs on Core 1. Do not call safeState()/forceAllOff(),
      // because those are Core-0 request wrappers and can deadlock waiting for a
      // Core-1 response while Core 1 is inside this function. Drive the hardware
      // safe directly here.
      fatalSafeStateActive = true;
      forceAllOffPhysical();
      setLastError(reason);
      setLedMode(LED_FAULT);
      return false;
    }
  }

  return true;
}

/**
 * @brief Configure shift-register GPIO pins, pulse SRCLR, and prepare known idle levels.
 */
void setupShiftRegisters() {
  shiftRegistersReady = false;
  outputsKnownSafe = false;

  Serial.println("STEP 1: setup shift-register pin modes");
  Serial.flush();

  configureStableSrOutput(SR_DATA);
  digitalWrite(SR_DATA, LOW);
  Serial.println("  SR_DATA GP11 OK");

  configureStableSrOutput(SR_CLOCK);
  digitalWrite(SR_CLOCK, LOW);
  Serial.println("  SR_CLOCK GP12 OK");

  configureStableSrOutput(SR_RESET);
  digitalWrite(SR_RESET, HIGH);
  Serial.println("  SR_RESET GP15 OK");

  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    configureStableSrOutput(SR_LATCH_PINS[ch]);
    digitalWrite(SR_LATCH_PINS[ch], LOW);

    Serial.print("  LATCH CH");
    Serial.print(ch + 1);
    Serial.print(" GP");
    Serial.print(SR_LATCH_PINS[ch]);
    Serial.println(" OK");
  }
  Serial.flush();

  srBusIdle();
  delayMicroseconds(100);

  Serial.println("  SRCLR low pulse");
  Serial.flush();
  pulseShiftRegisterClear();

  // Only now is the physical chain in a known cleared/idle state.
  shiftRegistersReady = true;
}

/**
 * @brief Physically latch 0x0000 into all eight output channels.
 */
void forceAllOffPhysical() {
  if (!shiftRegistersReady) {
    outputsKnownSafe = false;
    setLastError("-900,\"shift registers not ready\"");
    return;
  }

  outputsKnownSafe = false;
  setLedMode(LED_ACTIVITY);

  setAllLatchesLow();
  srBusIdle();

  // Clear the shift-register chain, then explicitly latch zero into every
  // channel output register. The repeated per-channel latch is required because
  // every channel has its own ST_CP/latch line.
  noInterrupts();
  pulseShiftRegisterClear();
  srBusIdle();
  interrupts();

  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    latchMaskToChannel(ch, 0x0000);
    channelMask[ch] = 0x0000;
  }

  outputsKnownSafe = true;
  setStatus("All channels OFF");
  if (!fatalSafeStateActive) {
    setLedMode(LED_OK);
  }

  srLog("Shift registers: all OFF done");
}
