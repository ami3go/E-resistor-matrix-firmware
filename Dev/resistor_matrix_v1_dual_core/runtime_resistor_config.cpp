/**
 * @file runtime_resistor_config.cpp
 * @brief Runtime resistor table configuration loader and parser for LittleFS-stored per-channel calibration tables.
 */

#include "app.h"

// 02_runtime_resistor_config.ino
// Split from rp2040_w5500_resistor_matrix_v1_safety_logic.ino.
// Keep all files in the same Arduino sketch folder.

// ============================================================
// Runtime resistor configuration
// ============================================================

/**
 * @brief Copy compile-time resistor branch definitions into the mutable runtime table.
 */
void copyDefaultConfigToRuntime() {
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    for (uint8_t i = 0; i < BIT_COUNT; i++) {
      channelResistorTable[ch][i].bit = CHANNEL_DEFAULT_TABLES[ch][i].bit;

      strncpy(
        channelResistorTable[ch][i].mosfet_name,
        CHANNEL_DEFAULT_TABLES[ch][i].mosfet_name,
        sizeof(channelResistorTable[ch][i].mosfet_name) - 1
      );
      channelResistorTable[ch][i].mosfet_name[sizeof(channelResistorTable[ch][i].mosfet_name) - 1] = '\0';

      strncpy(
        channelResistorTable[ch][i].nominal_resistance,
        CHANNEL_DEFAULT_TABLES[ch][i].nominal_resistance,
        sizeof(channelResistorTable[ch][i].nominal_resistance) - 1
      );
      channelResistorTable[ch][i].nominal_resistance[sizeof(channelResistorTable[ch][i].nominal_resistance) - 1] = '\0';
    }
  }
}

/**
 * @brief Get Runtime Resistor Info For Bit.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @param bit Function parameter.
 * @return Result value; for bool, true means the operation succeeded.
 */
RuntimeResistorInfo* getRuntimeResistorInfoForBit(uint8_t channelIndex, uint8_t bit) {
  if (channelIndex >= CHANNEL_COUNT) {
    return nullptr;
  }

  for (uint8_t i = 0; i < BIT_COUNT; i++) {
    if (channelResistorTable[channelIndex][i].bit == bit) {
      return &channelResistorTable[channelIndex][i];
    }
  }

  return nullptr;
}

/**
 * @brief Channel Config Path.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @return Result value; for bool, true means the operation succeeded.
 */
String channelConfigPath(uint8_t channelIndex) {
  return "/ch" + String(channelIndex + 1) + ".csv";
}

/**
 * @brief Network Config Path.
 * @return Result value; for bool, true means the operation succeeded.
 */
String networkConfigPath() {
  return "/network.csv";
}

/**
 * @brief Channel Config To Text.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @return Result value; for bool, true means the operation succeeded.
 */
String channelConfigToText(uint8_t channelIndex) {
  String out;
  out.reserve(450);

  out += "bit,mosfet_name,nominal_resistance\n";

  if (channelIndex >= CHANNEL_COUNT) {
    return out;
  }

  for (uint8_t i = 0; i < BIT_COUNT; i++) {
    out += String(channelResistorTable[channelIndex][i].bit);
    out += ",";
    out += channelResistorTable[channelIndex][i].mosfet_name;
    out += ",";
    out += channelResistorTable[channelIndex][i].nominal_resistance;
    out += "\n";
  }

  return out;
}

/**
 * @brief Parse Csv Config Line.
 * @param line Function parameter.
 * @param out Function parameter.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool parseCsvConfigLine(const String& line, RuntimeResistorInfo& out) {
  int c1 = line.indexOf(',');
  if (c1 < 0) {
    return false;
  }

  int c2 = line.indexOf(',', c1 + 1);
  if (c2 < 0) {
    return false;
  }

  String bitStr = line.substring(0, c1);
  String nameStr = line.substring(c1 + 1, c2);
  String rStr = line.substring(c2 + 1);

  bitStr.trim();
  nameStr.trim();
  rStr.trim();

  if (bitStr.length() == 0 || nameStr.length() == 0 || rStr.length() == 0) {
    return false;
  }

  int bit = bitStr.toInt();
  if (bit < 0 || bit > 15) {
    return false;
  }

  out.bit = uint8_t(bit);

  strncpy(out.mosfet_name, nameStr.c_str(), sizeof(out.mosfet_name) - 1);
  out.mosfet_name[sizeof(out.mosfet_name) - 1] = '\0';

  strncpy(out.nominal_resistance, rStr.c_str(), sizeof(out.nominal_resistance) - 1);
  out.nominal_resistance[sizeof(out.nominal_resistance) - 1] = '\0';

  return true;
}

/**
 * @brief Parse Header Initializer Line.
 * @param line Function parameter.
 * @param out Function parameter.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool parseHeaderInitializerLine(const String& line, RuntimeResistorInfo& out) {
  /*
    Accepts lines like:
      {0,  "Q16", "626R"},
      {15, "Q1",  "20M"},
  */
  int open = line.indexOf('{');
  int close = line.indexOf('}');
  if (open < 0 || close < 0 || close <= open) {
    return false;
  }

  String inner = line.substring(open + 1, close);
  int c1 = inner.indexOf(',');
  if (c1 < 0) {
    return false;
  }
  int c2 = inner.indexOf(',', c1 + 1);
  if (c2 < 0) {
    return false;
  }

  String bitStr = inner.substring(0, c1);
  String nameStr = inner.substring(c1 + 1, c2);
  String rStr = inner.substring(c2 + 1);

  bitStr.trim();
  nameStr.trim();
  rStr.trim();

  nameStr.replace("\"", "");
  rStr.replace("\"", "");

  if (bitStr.length() == 0 || nameStr.length() == 0 || rStr.length() == 0) {
    return false;
  }

  int bit = bitStr.toInt();
  if (bit < 0 || bit > 15) {
    return false;
  }

  out.bit = uint8_t(bit);

  strncpy(out.mosfet_name, nameStr.c_str(), sizeof(out.mosfet_name) - 1);
  out.mosfet_name[sizeof(out.mosfet_name) - 1] = '\0';

  strncpy(out.nominal_resistance, rStr.c_str(), sizeof(out.nominal_resistance) - 1);
  out.nominal_resistance[sizeof(out.nominal_resistance) - 1] = '\0';

  return true;
}

/**
 * @brief Parse Channel Config Text.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @param text Function parameter.
 * @param error Output buffer for a human-readable diagnostic message.
 * @param errorLen Output buffer for a human-readable diagnostic message.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool parseChannelConfigText(uint8_t channelIndex, const String& text, char* error, size_t errorLen) {
  if (channelIndex >= CHANNEL_COUNT) {
    snprintf(error, errorLen, "Invalid channel");
    return false;
  }

  RuntimeResistorInfo temp[BIT_COUNT];
  bool seen[BIT_COUNT] = {false};

  for (uint8_t i = 0; i < BIT_COUNT; i++) {
    temp[i].bit = i;
    snprintf(temp[i].mosfet_name, sizeof(temp[i].mosfet_name), "Q%u", unsigned(16 - i));
    snprintf(temp[i].nominal_resistance, sizeof(temp[i].nominal_resistance), "0R");
  }

  uint8_t validLines = 0;
  int start = 0;

  while (start < int(text.length())) {
    int end = text.indexOf('\n', start);
    if (end < 0) {
      end = text.length();
    }

    String line = text.substring(start, end);
    line.trim();

    start = end + 1;

    if (line.length() == 0) {
      continue;
    }

    if (line.startsWith("#") || line.startsWith("//")) {
      continue;
    }

    String upper = line;
    upper.toUpperCase();
    if (upper.startsWith("BIT,")) {
      continue;
    }

    RuntimeResistorInfo info;
    bool parsed = false;

    if (line.indexOf('{') >= 0) {
      parsed = parseHeaderInitializerLine(line, info);
    } else if (line.indexOf(',') >= 0) {
      parsed = parseCsvConfigLine(line, info);
    }

    if (!parsed) {
      continue;
    }

    if (seen[info.bit]) {
      snprintf(error, errorLen, "Duplicate bit %u", unsigned(info.bit));
      return false;
    }

    seen[info.bit] = true;
    temp[info.bit] = info;
    validLines++;
  }

  if (validLines != BIT_COUNT) {
    snprintf(error, errorLen, "Expected 16 valid bit lines, got %u", unsigned(validLines));
    return false;
  }

  for (uint8_t bit = 0; bit < BIT_COUNT; bit++) {
    if (!seen[bit]) {
      snprintf(error, errorLen, "Missing bit %u", unsigned(bit));
      return false;
    }
  }

  for (uint8_t i = 0; i < BIT_COUNT; i++) {
    channelResistorTable[channelIndex][i] = temp[i];
  }

  snprintf(error, errorLen, "OK");
  return true;
}

/**
 * @brief Save Channel Config To Little FS.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool saveChannelConfigToLittleFS(uint8_t channelIndex) {
  if (!littleFsReady || channelIndex >= CHANNEL_COUNT) {
    return false;
  }

  File f = LittleFS.open(channelConfigPath(channelIndex), "w");
  if (!f) {
    return false;
  }

  String text = channelConfigToText(channelIndex);
  size_t written = f.print(text);
  f.close();

  return written == text.length();
}

/**
 * @brief Load Channel Config From Little FS.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool loadChannelConfigFromLittleFS(uint8_t channelIndex) {
  if (!littleFsReady || channelIndex >= CHANNEL_COUNT) {
    return false;
  }

  String path = channelConfigPath(channelIndex);
  if (!LittleFS.exists(path)) {
    return false;
  }

  File f = LittleFS.open(path, "r");
  if (!f) {
    return false;
  }

  String text = f.readString();
  f.close();

  char error[96];
  return parseChannelConfigText(channelIndex, text, error, sizeof(error));
}

/**
 * @brief Load all per-channel runtime resistor configuration files from LittleFS.
 */
void loadAllRuntimeConfigs() {
  copyDefaultConfigToRuntime();

  if (!littleFsReady) {
    Serial.println("LittleFS not ready: using compile-time resistor defaults");
    return;
  }

  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    if (loadChannelConfigFromLittleFS(ch)) {
      Serial.print("Loaded runtime config for CH");
      Serial.println(ch + 1);
    }
  }
}

