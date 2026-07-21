/**
 * @file runtime_resistor_config.cpp
 * @brief Numeric runtime calibration model and LittleFS text compatibility layer.
 */

#include "app.h"
#include <math.h>

namespace {

constexpr float MIN_CALIBRATION_OHM = 0.001f;
constexpr float MAX_CALIBRATION_OHM = 1.0e9f;

const char* const MOSFET_NAMES[BIT_COUNT] = {
  "Q16", "Q15", "Q14", "Q13", "Q12", "Q11", "Q10", "Q9",
  "Q8", "Q7", "Q6", "Q5", "Q4", "Q3", "Q2", "Q1"
};

bool parseStrictBitField(String text, uint8_t& bit) {
  text.trim();
  if (text.length() == 0) return false;
  for (size_t i = 0; i < text.length(); ++i) {
    if (!isdigit(text[i])) return false;
  }
  long value = strtol(text.c_str(), nullptr, 10);
  if (value < 0 || value >= BIT_COUNT) return false;
  bit = uint8_t(value);
  return true;
}

bool parseCalibrationResistance(String text, float& resistanceOhm) {
  text.trim();
  double parsed = 0.0;
  if (!parseResistanceOhms(text.c_str(), parsed)) return false;
  if (!isfinite(parsed) || parsed < MIN_CALIBRATION_OHM || parsed > MAX_CALIBRATION_OHM) {
    return false;
  }
  resistanceOhm = float(parsed);
  return isfinite(resistanceOhm) && resistanceOhm > 0.0f;
}

String calibrationValueText(float resistanceOhm) {
  if (!isfinite(resistanceOhm) || resistanceOhm <= 0.0f) return "NaN";
  return String(double(resistanceOhm), 6);
}

String resistorTableToText(const float table[BIT_COUNT]) {
  String out;
  out.reserve(600);
  out += "bit,mosfet_name,nominal_resistance\n";
  for (uint8_t bit = 0; bit < BIT_COUNT; ++bit) {
    out += String(bit);
    out += ",";
    out += MOSFET_NAMES[bit];
    out += ",";
    out += calibrationValueText(table[bit]);
    out += "\n";
  }
  return out;
}

bool parseChannelConfigTextToTable(
  const String& text,
  float output[BIT_COUNT],
  char* error,
  size_t errorLen
) {
  float temp[BIT_COUNT] = {};
  bool seen[BIT_COUNT] = {false};
  uint8_t validLines = 0;
  int start = 0;

  while (start < int(text.length())) {
    int end = text.indexOf('\n', start);
    if (end < 0) end = text.length();

    String line = text.substring(start, end);
    line.trim();
    start = end + 1;

    if (line.length() == 0 || line.startsWith("#") || line.startsWith("//")) continue;

    String upper = line;
    upper.toUpperCase();
    if (upper.startsWith("BIT,")) continue;

    ParsedResistorInfo info{};
    bool parsed = false;
    if (line.indexOf('{') >= 0) {
      parsed = parseHeaderInitializerLine(line, info);
    } else if (line.indexOf(',') >= 0) {
      parsed = parseCsvConfigLine(line, info);
    }

    if (!parsed) {
      snprintf(error, errorLen, "Malformed calibration line near byte %d", start);
      return false;
    }
    if (seen[info.bit]) {
      snprintf(error, errorLen, "Duplicate bit %u", unsigned(info.bit));
      return false;
    }

    seen[info.bit] = true;
    temp[info.bit] = info.resistanceOhm;
    validLines++;
  }

  if (validLines != BIT_COUNT) {
    snprintf(error, errorLen, "Expected %u valid bit lines, got %u", unsigned(BIT_COUNT), unsigned(validLines));
    return false;
  }

  for (uint8_t bit = 0; bit < BIT_COUNT; ++bit) {
    if (!seen[bit]) {
      snprintf(error, errorLen, "Missing bit %u", unsigned(bit));
      return false;
    }
    output[bit] = temp[bit];
  }

  snprintf(error, errorLen, "OK");
  return true;
}

uint8_t countBundleMarker(const String& text, const String& marker) {
  uint8_t count = 0;
  int position = 0;
  while (position >= 0 && position < int(text.length())) {
    position = text.indexOf(marker, position);
    if (position < 0) break;
    count++;
    position += marker.length();
  }
  return count;
}

bool writeImportFileExact(const String& path, const String& text) {
  LittleFS.remove(path);
  File file = LittleFS.open(path, "w");
  if (!file) return false;
  size_t written = file.print(text);
  file.close();
  if (written != text.length()) {
    LittleFS.remove(path);
    return false;
  }

  File verify = LittleFS.open(path, "r");
  if (!verify) {
    LittleFS.remove(path);
    return false;
  }
  size_t storedSize = size_t(verify.size());
  verify.close();
  if (storedSize != text.length()) {
    LittleFS.remove(path);
    return false;
  }
  return true;
}

} // namespace

const char* mosfetNameForBit(uint8_t bit) {
  return bit < BIT_COUNT ? MOSFET_NAMES[bit] : "-";
}

float getRuntimeResistanceOhms(uint8_t channelIndex, uint8_t bit) {
  if (channelIndex >= CHANNEL_COUNT || bit >= BIT_COUNT) return NAN;
  return channelResistorOhms[channelIndex][bit];
}

void copyDefaultConfigToRuntime() {
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
    for (uint8_t bit = 0; bit < BIT_COUNT; ++bit) {
      channelResistorOhms[ch][bit] = DEFAULT_RESISTOR_OHMS[bit];
    }
  }
  invalidateConductanceCache();
}

String channelConfigPath(uint8_t channelIndex) {
  return "/ch" + String(channelIndex + 1) + ".csv";
}

String networkConfigPath() {
  return "/network.csv";
}

String channelConfigToText(uint8_t channelIndex) {
  if (channelIndex >= CHANNEL_COUNT) return String("bit,mosfet_name,nominal_resistance\n");
  return resistorTableToText(channelResistorOhms[channelIndex]);
}

bool getChannelConfigStorageInfo(uint8_t channelIndex, bool& exists, size_t& sizeBytes) {
  exists = false;
  sizeBytes = 0;
  if (channelIndex >= CHANNEL_COUNT) return false;
  if (!littleFsReady) return true;

  String path = channelConfigPath(channelIndex);
  exists = LittleFS.exists(path);
  if (!exists) return true;

  File file = LittleFS.open(path, "r");
  if (!file) {
    exists = false;
    return true;
  }
  sizeBytes = size_t(file.size());
  file.close();
  return true;
}

String calibrationFileListText() {
  String out;
  out.reserve(520);
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
    bool exists = false;
    size_t sizeBytes = 0;
    getChannelConfigStorageInfo(ch, exists, sizeBytes);
    if (ch > 0) out += ";";
    out += "CH";
    out += String(ch + 1);
    out += ",path=";
    out += channelConfigPath(ch);
    out += ",saved=";
    out += exists ? "1" : "0";
    out += ",size=";
    out += String(uint32_t(sizeBytes));
  }
  return out;
}

String allChannelConfigsToBundleText() {
  String out;
  out.reserve(6200);
  out += "# E-Resistor calibration bundle\n";
  out += "# format=1\n";
  out += "# firmware=";
  out += FIRMWARE_VERSION;
  out += "\n# serial=";
  out += deviceSerialNumber;
  out += "\n";

  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
    bool exists = false;
    size_t sizeBytes = 0;
    getChannelConfigStorageInfo(ch, exists, sizeBytes);
    out += "#BEGIN CH";
    out += String(ch + 1);
    out += " path=";
    out += channelConfigPath(ch);
    out += " saved=";
    out += exists ? "1" : "0";
    out += " size=";
    out += String(uint32_t(sizeBytes));
    out += "\n";
    out += channelConfigToText(ch);
    out += "#END CH";
    out += String(ch + 1);
    out += "\n";
  }
  return out;
}

bool parseCsvConfigLine(const String& line, ParsedResistorInfo& out) {
  int c1 = line.indexOf(',');
  if (c1 < 0) return false;
  int c2 = line.indexOf(',', c1 + 1);
  if (c2 < 0) return false;

  String bitText = line.substring(0, c1);
  String nameText = line.substring(c1 + 1, c2);
  String resistanceText = line.substring(c2 + 1);
  bitText.trim();
  nameText.trim();
  resistanceText.trim();
  if (nameText.length() == 0) return false;

  uint8_t bit = 0;
  float resistanceOhm = 0.0f;
  if (!parseStrictBitField(bitText, bit)) return false;
  if (!parseCalibrationResistance(resistanceText, resistanceOhm)) return false;

  out.bit = bit;
  out.resistanceOhm = resistanceOhm;
  return true;
}

bool parseHeaderInitializerLine(const String& line, ParsedResistorInfo& out) {
  int open = line.indexOf('{');
  int close = line.indexOf('}');
  if (open < 0 || close < 0 || close <= open) return false;

  String inner = line.substring(open + 1, close);
  int c1 = inner.indexOf(',');
  if (c1 < 0) return false;
  int c2 = inner.indexOf(',', c1 + 1);
  if (c2 < 0) return false;

  String bitText = inner.substring(0, c1);
  String nameText = inner.substring(c1 + 1, c2);
  String resistanceText = inner.substring(c2 + 1);
  bitText.trim();
  nameText.trim();
  resistanceText.trim();
  nameText.replace("\"", "");
  resistanceText.replace("\"", "");
  if (nameText.length() == 0) return false;

  uint8_t bit = 0;
  float resistanceOhm = 0.0f;
  if (!parseStrictBitField(bitText, bit)) return false;
  if (!parseCalibrationResistance(resistanceText, resistanceOhm)) return false;

  out.bit = bit;
  out.resistanceOhm = resistanceOhm;
  return true;
}

bool parseChannelConfigText(uint8_t channelIndex, const String& text, char* error, size_t errorLen) {
  if (channelIndex >= CHANNEL_COUNT) {
    snprintf(error, errorLen, "Invalid channel");
    return false;
  }

  float parsed[BIT_COUNT] = {};
  if (!parseChannelConfigTextToTable(text, parsed, error, errorLen)) return false;
  for (uint8_t bit = 0; bit < BIT_COUNT; ++bit) {
    channelResistorOhms[channelIndex][bit] = parsed[bit];
  }
  invalidateConductanceCache();
  snprintf(error, errorLen, "OK");
  return true;
}

bool restoreAllChannelConfigsFromBundleText(const String& text, char* error, size_t errorLen) {
  if (!littleFsReady) {
    snprintf(error, errorLen, "LittleFS is not ready");
    return false;
  }
  if (countBundleMarker(text, "#BEGIN CH") != CHANNEL_COUNT ||
      countBundleMarker(text, "#END CH") != CHANNEL_COUNT) {
    snprintf(error, errorLen, "Bundle must contain exactly %u BEGIN and END markers", unsigned(CHANNEL_COUNT));
    return false;
  }

  static float candidate[CHANNEL_COUNT][BIT_COUNT];
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
    String beginMarker = "#BEGIN CH" + String(ch + 1);
    String endMarker = "#END CH" + String(ch + 1);
    int begin = text.indexOf(beginMarker);
    if (begin < 0 || text.indexOf(beginMarker, begin + beginMarker.length()) >= 0) {
      snprintf(error, errorLen, "Missing or duplicate BEGIN marker for CH%u", unsigned(ch + 1));
      return false;
    }
    int dataStart = text.indexOf('\n', begin);
    if (dataStart < 0) {
      snprintf(error, errorLen, "Missing data after CH%u BEGIN marker", unsigned(ch + 1));
      return false;
    }
    dataStart++;
    int end = text.indexOf(endMarker, dataStart);
    if (end < 0 || text.indexOf(endMarker, end + endMarker.length()) >= 0) {
      snprintf(error, errorLen, "Missing or duplicate END marker for CH%u", unsigned(ch + 1));
      return false;
    }

    String tableText = text.substring(dataStart, end);
    char channelError[96];
    if (!parseChannelConfigTextToTable(tableText, candidate[ch], channelError, sizeof(channelError))) {
      snprintf(error, errorLen, "CH%u: %s", unsigned(ch + 1), channelError);
      return false;
    }
  }

  bool backupCreated[CHANNEL_COUNT] = {false};
  bool finalInstalled[CHANNEL_COUNT] = {false};

  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
    String finalPath = channelConfigPath(ch);
    String tempPath = finalPath + ".import.tmp";
    String backupPath = finalPath + ".import.bak";
    LittleFS.remove(tempPath);
    if (LittleFS.exists(backupPath)) {
      if (LittleFS.exists(finalPath)) {
        LittleFS.remove(backupPath);
      } else if (!LittleFS.rename(backupPath.c_str(), finalPath.c_str())) {
        snprintf(error, errorLen, "Failed to recover previous CH%u calibration backup", unsigned(ch + 1));
        return false;
      }
    }
  }

  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
    String tempPath = channelConfigPath(ch) + ".import.tmp";
    String normalized = resistorTableToText(candidate[ch]);
    if (!writeImportFileExact(tempPath, normalized)) {
      for (uint8_t i = 0; i < CHANNEL_COUNT; ++i) LittleFS.remove(channelConfigPath(i) + ".import.tmp");
      snprintf(error, errorLen, "Failed to stage CH%u calibration file", unsigned(ch + 1));
      return false;
    }
  }

  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
    String finalPath = channelConfigPath(ch);
    String backupPath = finalPath + ".import.bak";
    LittleFS.remove(backupPath);
    if (LittleFS.exists(finalPath)) {
      if (!LittleFS.rename(finalPath.c_str(), backupPath.c_str())) {
        for (uint8_t i = 0; i < ch; ++i) {
          if (backupCreated[i]) LittleFS.rename((channelConfigPath(i) + ".import.bak").c_str(), channelConfigPath(i).c_str());
        }
        for (uint8_t i = 0; i < CHANNEL_COUNT; ++i) LittleFS.remove(channelConfigPath(i) + ".import.tmp");
        snprintf(error, errorLen, "Failed to prepare CH%u file replacement", unsigned(ch + 1));
        return false;
      }
      backupCreated[ch] = true;
    }
  }

  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
    String finalPath = channelConfigPath(ch);
    String tempPath = finalPath + ".import.tmp";
    if (!LittleFS.rename(tempPath.c_str(), finalPath.c_str())) {
      for (uint8_t i = 0; i < CHANNEL_COUNT; ++i) {
        String restoreFinal = channelConfigPath(i);
        String restoreBackup = restoreFinal + ".import.bak";
        if (finalInstalled[i]) LittleFS.remove(restoreFinal);
        if (backupCreated[i] && LittleFS.exists(restoreBackup)) LittleFS.rename(restoreBackup.c_str(), restoreFinal.c_str());
        LittleFS.remove(restoreFinal + ".import.tmp");
      }
      snprintf(error, errorLen, "Failed to install CH%u calibration file", unsigned(ch + 1));
      return false;
    }
    finalInstalled[ch] = true;
  }

  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
    for (uint8_t bit = 0; bit < BIT_COUNT; ++bit) channelResistorOhms[ch][bit] = candidate[ch][bit];
    LittleFS.remove(channelConfigPath(ch) + ".import.bak");
  }

  rebuildConductanceCache();
  snprintf(error, errorLen, "OK");
  return true;
}

bool saveChannelConfigToLittleFS(uint8_t channelIndex) {
  if (!littleFsReady || channelIndex >= CHANNEL_COUNT) return false;
  File file = LittleFS.open(channelConfigPath(channelIndex), "w");
  if (!file) return false;
  String text = channelConfigToText(channelIndex);
  size_t written = file.print(text);
  file.close();
  return written == text.length();
}

bool loadChannelConfigFromLittleFS(uint8_t channelIndex) {
  if (!littleFsReady || channelIndex >= CHANNEL_COUNT) return false;
  String path = channelConfigPath(channelIndex);
  if (!LittleFS.exists(path)) return false;
  File file = LittleFS.open(path, "r");
  if (!file) return false;
  String text = file.readString();
  file.close();
  char error[96];
  return parseChannelConfigText(channelIndex, text, error, sizeof(error));
}

bool allChannelsHaveSavedCalibration() {
  const uint8_t requiredMask = uint8_t((1U << CHANNEL_COUNT) - 1U);
  return calibrationSavedMask == requiredMask &&
         calibrationLoadedMask == requiredMask &&
         calibrationLoadErrorMask == 0U;
}

String calibrationStorageStatusText() {
  String out;
  out.reserve(96);
  out += "saved_mask=0x";
  if (calibrationSavedMask < 0x10U) out += "0";
  out += String(calibrationSavedMask, HEX);
  out += ",loaded_mask=0x";
  if (calibrationLoadedMask < 0x10U) out += "0";
  out += String(calibrationLoadedMask, HEX);
  out += ",error_mask=0x";
  if (calibrationLoadErrorMask < 0x10U) out += "0";
  out += String(calibrationLoadErrorMask, HEX);
  out += ",all_saved=";
  out += allChannelsHaveSavedCalibration() ? "1" : "0";
  return out;
}

void loadAllRuntimeConfigs() {
  copyDefaultConfigToRuntime();
  calibrationSavedMask = 0U;
  calibrationLoadedMask = 0U;
  calibrationLoadErrorMask = 0U;

  if (!littleFsReady) {
    Serial.println("LittleFS not ready: using compile-time resistor defaults");
    appendLogEvent("WARNING: LittleFS unavailable; nominal resistor defaults active");
    rebuildConductanceCache();
    return;
  }

  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
    const uint8_t channelBit = uint8_t(1U << ch);
    const String path = channelConfigPath(ch);
    const bool exists = LittleFS.exists(path);
    if (exists) calibrationSavedMask |= channelBit;

    if (exists && loadChannelConfigFromLittleFS(ch)) {
      calibrationLoadedMask |= channelBit;
      Serial.print("Loaded runtime calibration for CH");
      Serial.println(ch + 1);
    } else if (exists) {
      calibrationLoadErrorMask |= channelBit;
      char message[112];
      snprintf(message, sizeof(message), "WARNING: CH%u calibration file exists but is invalid; nominal defaults active", unsigned(ch + 1U));
      Serial.println(message);
      appendLogEvent(message);
    } else {
      char message[96];
      snprintf(message, sizeof(message), "WARNING: CH%u has no saved calibration; nominal defaults active", unsigned(ch + 1U));
      Serial.println(message);
      appendLogEvent(message);
    }
  }

  rebuildConductanceCache();
  const String status = calibrationStorageStatusText();
  Serial.print("Calibration storage: ");
  Serial.println(status);
  appendLogEvent((String("Calibration storage: ") + status).c_str());
}
