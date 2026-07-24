/**
 * @file parse_helpers.cpp
 * @brief HTTP query argument parsing helpers for channels, bit indexes, and 16-bit masks.
 */

#include "app.h"


// ============================================================
// HTTP / SCPI parsing helpers
// ============================================================

/**
 * @brief Parse Channel.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool parseChannel(uint8_t& channelIndex) {
  if (!server.hasArg("ch")) {
    return false;
  }

  int ch = server.arg("ch").toInt();

  if (ch < 1 || ch > int(CHANNEL_COUNT)) {
    return false;
  }

  channelIndex = uint8_t(ch - 1);
  return true;
}

/**
 * @brief Parse Bit.
 * @param bitIndex Function parameter.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool parseBit(uint8_t& bitIndex) {
  if (!server.hasArg("bit")) {
    return false;
  }

  int bit = server.arg("bit").toInt();

  if (bit < 0 || bit >= BIT_COUNT) {
    return false;
  }

  bitIndex = uint8_t(bit);
  return true;
}

/**
 * @brief Parse a 16-bit mask from hexadecimal text with or without 0x prefix.
 * @param s Function parameter.
 * @param mask 16-bit resistance switch mask.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool parseHex16String(String s, uint16_t& mask) {
  s.trim();
  s.toUpperCase();

  if (s.startsWith("0X")) {
    s.remove(0, 2);
  }

  if (s.length() < 1 || s.length() > 4) {
    return false;
  }

  for (uint8_t i = 0; i < s.length(); i++) {
    if (!isxdigit(s[i])) {
      return false;
    }
  }

  unsigned long value = strtoul(s.c_str(), nullptr, 16);

  if (value > 0xFFFFUL) {
    return false;
  }

  mask = uint16_t(value);
  return true;
}

/**
 * @brief Parse Mask.
 * @param mask 16-bit resistance switch mask.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool parseMask(uint16_t& mask) {
  if (!server.hasArg("mask")) {
    return false;
  }

  return parseHex16String(server.arg("mask"), mask);
}

