/**
 * @file resistance_calculation.cpp
 * @brief Resistance parsing, equivalent-resistance calculation, mask safety checks, profile handling, and UI helper rendering.
 */

#include "app.h"

// 03_resistance_calculation.ino
// Split from rp2040_w5500_resistor_matrix_v1_safety_logic.ino.
// Keep all files in the same Arduino sketch folder.

// ============================================================
// Resistance calculation helpers
// ============================================================

/**
 * @brief Parse Resistance Ohms.
 * @param text Function parameter.
 * @param ohms Function parameter.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool parseResistanceOhms(const char* text, double& ohms) {
  if (text == nullptr) {
    return false;
  }

  String s(text);
  s.trim();
  s.toUpperCase();

  if (s.length() == 0) {
    return false;
  }

  if (s == "TODO" || s == "NC" || s == "N/A") {
    return false;
  }

  char* endPtr = nullptr;
  double value = strtod(s.c_str(), &endPtr);

  if (endPtr == s.c_str()) {
    return false;
  }

  while (*endPtr == ' ') {
    endPtr++;
  }

  double multiplier = 1.0;

  if (*endPtr == 'R' || *endPtr == '\0') {
    multiplier = 1.0;
  } else if (*endPtr == 'K') {
    multiplier = 1000.0;
  } else if (*endPtr == 'M') {
    multiplier = 1000000.0;
  } else {
    return false;
  }

  ohms = value * multiplier;

  if (ohms <= 0.0) {
    return false;
  }

  return true;
}

/**
 * @brief Format Resistance Ohms.
 * @param ohms Function parameter.
 * @return Result value; for bool, true means the operation succeeded.
 */
String formatResistanceOhms(double ohms) {
  if (ohms >= 10000000.0) {
    return String(ohms / 1000000.0, 2) + " MOhm";
  }

  if (ohms >= 1000000.0) {
    return String(ohms / 1000000.0, 3) + " MOhm";
  }

  if (ohms >= 10000.0) {
    return String(ohms / 1000.0, 2) + " kOhm";
  }

  if (ohms >= 1000.0) {
    return String(ohms / 1000.0, 3) + " kOhm";
  }

  return String(ohms, 2) + " Ohm";
}

/**
 * @brief Calculate Output Resistance Text.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @param mask 16-bit resistance switch mask.
 * @return Result value; for bool, true means the operation succeeded.
 */
String calculateOutputResistanceText(uint8_t channelIndex, uint16_t mask) {
  if (channelIndex >= CHANNEL_COUNT) {
    return "CONFIG ERROR";
  }

  if (mask == 0x0000) {
    return "OPEN";
  }

  double inverseSum = 0.0;
  bool hasKnownBranch = false;

  for (uint8_t bit = 0; bit < BIT_COUNT; bit++) {
    if ((mask & (uint16_t(1) << bit)) == 0) {
      continue;
    }

    RuntimeResistorInfo* info = getRuntimeResistorInfoForBit(channelIndex, bit);

    if (info == nullptr) {
      return "CONFIG ERROR";
    }

    double rOhm = 0.0;

    if (!parseResistanceOhms(info->nominal_resistance, rOhm)) {
      return "CONFIG ERROR";
    }

    inverseSum += 1.0 / rOhm;
    hasKnownBranch = true;
  }

  if (!hasKnownBranch || inverseSum <= 0.0) {
    return "OPEN";
  }

  double equivalentOhms = 1.0 / inverseSum;
  return formatResistanceOhms(equivalentOhms);
}


/**
 * @brief Calculate the equivalent resistance of all active branches in one channel mask.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @param mask 16-bit resistance switch mask.
 * @param outOhms Function parameter.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool calculateEquivalentOhms(uint8_t channelIndex, uint16_t mask, double& outOhms) {
  if (channelIndex >= CHANNEL_COUNT) {
    return false;
  }

  if (mask == 0x0000) {
    outOhms = INFINITY;
    return true;
  }

  double inverseSum = 0.0;

  for (uint8_t bit = 0; bit < BIT_COUNT; bit++) {
    if ((mask & (uint16_t(1) << bit)) == 0) {
      continue;
    }

    RuntimeResistorInfo* info = getRuntimeResistorInfoForBit(channelIndex, bit);
    if (info == nullptr) {
      return false;
    }

    double rOhm = 0.0;
    if (!parseResistanceOhms(info->nominal_resistance, rOhm)) {
      return false;
    }

    inverseSum += 1.0 / rOhm;
  }

  if (inverseSum <= 0.0) {
    outOhms = INFINITY;
    return true;
  }

  outOhms = 1.0 / inverseSum;
  return true;
}

/**
 * @brief Count Active Bits16.
 * @param mask 16-bit resistance switch mask.
 * @return Result value; for bool, true means the operation succeeded.
 */
uint8_t countActiveBits16(uint16_t mask) {
  uint8_t count = 0;
  while (mask != 0) {
    count += mask & 1U;
    mask >>= 1;
  }
  return count;
}

/**
 * @brief Validate a mask against the configured minimum resistance and maximum active-bit limits.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @param mask 16-bit resistance switch mask.
 * @param reason Output buffer for a human-readable diagnostic message.
 * @param reasonLen Output buffer for a human-readable diagnostic message.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool checkMaskSafety(uint8_t channelIndex, uint16_t mask, char* reason, size_t reasonLen) {
  if (mask == 0x0000) {
    return true;
  }

  uint8_t activeBits = countActiveBits16(mask);

  if (!safetyExpertMode && activeBits > safetyMaxActiveBits) {
    snprintf(reason, reasonLen, "Rejected: %u active bits exceeds safety limit %u", unsigned(activeBits), unsigned(safetyMaxActiveBits));
    return false;
  }

  double ohms = 0.0;
  if (!calculateEquivalentOhms(channelIndex, mask, ohms)) {
    snprintf(reason, reasonLen, "Rejected: resistance calculation failed");
    return false;
  }

  if (!safetyExpertMode && isfinite(ohms) && ohms < safetyMinOhm) {
    snprintf(reason, reasonLen, "Rejected: calculated resistance %.3f Ohm below safety limit %.3f Ohm", ohms, safetyMinOhm);
    return false;
  }

  return true;
}

/**
 * @brief Resistance Css Class.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @param mask 16-bit resistance switch mask.
 * @return Result value; for bool, true means the operation succeeded.
 */
String resistanceCssClass(uint8_t channelIndex, uint16_t mask) {
  if (mask == 0x0000) {
    return "res-open";
  }

  double ohms = 0.0;
  if (!calculateEquivalentOhms(channelIndex, mask, ohms) || !isfinite(ohms)) {
    return "res-error";
  }

  if (ohms < 500.0) {
    return "res-danger";
  }
  if (ohms < 10000.0) {
    return "res-warn";
  }
  if (ohms <= 1000000.0) {
    return "res-ok";
  }
  return "res-high";
}

/**
 * @brief Safety Config Path.
 * @return Result value; for bool, true means the operation succeeded.
 */
String safetyConfigPath() {
  return "/safety.csv";
}

/**
 * @brief Calibration Meta Path.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @return Result value; for bool, true means the operation succeeded.
 */
String calibrationMetaPath(uint8_t channelIndex) {
  return String("/meta_ch") + String(channelIndex + 1) + ".csv";
}

/**
 * @brief Append Log Event.
 * @param text Function parameter.
 */
void appendLogEvent(const char* text) {
  if (text == nullptr) {
    return;
  }

  uint8_t index = eventLogHead;
  snprintf(eventLog[index], sizeof(eventLog[index]), "%lu ms,%s", (unsigned long)millis(), text);

  eventLogHead = (eventLogHead + 1U) % 32U;
  if (eventLogCount < 32U) {
    eventLogCount++;
  }
}

/**
 * @brief Sanitize Name.
 * @param name Function parameter.
 * @return Result value; for bool, true means the operation succeeded.
 */
String sanitizeName(String name) {
  name.trim();

  String out;
  out.reserve(24);

  for (uint16_t i = 0; i < name.length() && out.length() < 24; i++) {
    char c = name[i];
    if (isalnum(c) || c == '_' || c == '-') {
      out += c;
    } else if (c == ' ' || c == '.') {
      out += '_';
    }
  }

  if (out.length() == 0) {
    out = "profile";
  }

  return out;
}

/**
 * @brief Profile Path From Name.
 * @param name Function parameter.
 * @return Result value; for bool, true means the operation succeeded.
 */
String profilePathFromName(String name) {
  return String("/profile_") + sanitizeName(name) + ".csv";
}

/**
 * @brief Save Safety Config To Little FS.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool saveSafetyConfigToLittleFS() {
  if (!littleFsReady) {
    return false;
  }

  File f = LittleFS.open(safetyConfigPath(), "w");
  if (!f) {
    return false;
  }

  f.print("min_ohm,");
  f.println(String(safetyMinOhm, 6));
  f.print("max_active_bits,");
  f.println(String(safetyMaxActiveBits));
  f.print("expert_mode,");
  f.println(safetyExpertMode ? "1" : "0");
  f.close();
  return true;
}

/**
 * @brief Load Safety Config From Little FS.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool loadSafetyConfigFromLittleFS() {
  if (!littleFsReady || !LittleFS.exists(safetyConfigPath())) {
    return false;
  }

  File f = LittleFS.open(safetyConfigPath(), "r");
  if (!f) {
    return false;
  }

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();

    int comma = line.indexOf(',');
    if (comma < 0) {
      continue;
    }

    String key = line.substring(0, comma);
    String value = line.substring(comma + 1);
    key.trim();
    value.trim();
    key.toLowerCase();

    if (key == "min_ohm") {
      safetyMinOhm = value.toFloat();
    } else if (key == "max_active_bits") {
      int v = value.toInt();
      if (v >= 1 && v <= 16) {
        safetyMaxActiveBits = uint8_t(v);
      }
    } else if (key == "expert_mode") {
      safetyExpertMode = (value == "1" || value == "true" || value == "on");
    }
  }

  f.close();
  return true;
}

/**
 * @brief Search masks locally on the RP2040 to find the nearest output resistance.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @param targetOhm Function parameter.
 * @param bestMask 16-bit resistance switch mask.
 * @param bestOhm Function parameter.
 * @param bestErrorPercent Output buffer for a human-readable diagnostic message.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool findNearestMaskForTarget(uint8_t channelIndex, double targetOhm, uint16_t& bestMask, double& bestOhm, double& bestErrorPercent) {
  if (channelIndex >= CHANNEL_COUNT || targetOhm <= 0.0) {
    return false;
  }

  double bestScore = 1.0e99;
  bool found = false;

  for (uint32_t mask = 1; mask <= 0xFFFFUL; mask++) {
    char reason[96];
    if (!checkMaskSafety(channelIndex, uint16_t(mask), reason, sizeof(reason))) {
      continue;
    }

    double ohms = 0.0;
    if (!calculateEquivalentOhms(channelIndex, uint16_t(mask), ohms) || !isfinite(ohms) || ohms <= 0.0) {
      continue;
    }

    double score = fabs(log(ohms / targetOhm));
    if (score < bestScore) {
      bestScore = score;
      bestMask = uint16_t(mask);
      bestOhm = ohms;
      found = true;
    }
  }

  if (!found) {
    return false;
  }

  bestErrorPercent = (bestOhm - targetOhm) / targetOhm * 100.0;
  return true;
}

/**
 * @brief Profile List Options.
 * @return Result value; for bool, true means the operation succeeded.
 */
String profileListOptions() {
  String html;

  if (!littleFsReady) {
    html += "<option value=''>LittleFS not ready</option>";
    return html;
  }

  Dir dir = LittleFS.openDir("/");
  uint16_t count = 0;

  while (dir.next()) {
    String name = dir.fileName();
    if (!name.startsWith("/")) {
      name = "/" + name;
    }

    if (name.startsWith("/profile_") && name.endsWith(".csv")) {
      String label = name.substring(9, name.length() - 4);
      html += "<option value='";
      html += label;
      html += "'>";
      html += label;
      html += "</option>";
      count++;
    }
  }

  if (count == 0) {
    html += "<option value=''>No profiles saved</option>";
  }

  return html;
}

/**
 * @brief Load Profile Masks.
 * @param profileName Size of the associated output buffer in bytes.
 * @param masks 16-bit resistance switch mask.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool loadProfileMasks(String profileName, uint16_t masks[CHANNEL_COUNT]) {
  if (!littleFsReady) {
    return false;
  }

  String path = profilePathFromName(profileName);
  if (!LittleFS.exists(path)) {
    return false;
  }

  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    masks[ch] = channelMask[ch];
  }

  File f = LittleFS.open(path, "r");
  if (!f) {
    return false;
  }

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();

    int comma = line.indexOf(',');
    if (comma < 0) {
      continue;
    }

    String left = line.substring(0, comma);
    String right = line.substring(comma + 1);
    left.trim();
    right.trim();
    left.toUpperCase();

    if (!left.startsWith("CH")) {
      continue;
    }

    int ch = left.substring(2).toInt();
    uint16_t mask = 0;

    if (ch >= 1 && ch <= 8 && parseHex16String(right, mask)) {
      masks[ch - 1] = mask;
    }
  }

  f.close();
  return true;
}

/**
 * @brief Save Current Profile.
 * @param profileName Size of the associated output buffer in bytes.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool saveCurrentProfile(String profileName) {
  if (!littleFsReady) {
    return false;
  }

  String path = profilePathFromName(profileName);
  File f = LittleFS.open(path, "w");
  if (!f) {
    return false;
  }

  f.print("name,");
  f.println(sanitizeName(profileName));

  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    f.print("CH");
    f.print(ch + 1);
    f.print(",");
    f.println(hex16(channelMask[ch]));
  }

  f.close();
  return true;
}

/**
 * @brief Append Profile Manager.
 * @param html HTML string that receives generated markup.
 */
void appendProfileManager(String& html) {
  html += "<div class='card'>";
  html += "<h2>Profiles / Presets</h2>";
  html += "<p class='small'>Save, load, and delete full 8-channel mask states. Profiles are stored as LittleFS CSV files.</p>";

  html += "<form method='POST' action='/profile_save'>";
  html += "<input name='name' placeholder='Profile name' value='default_profile'>";
  html += "<button type='submit'>Save current state</button>";
  html += "</form>";

  html += "<form method='GET' action='/profile_apply'>";
  html += "<select name='name'>";
  html += profileListOptions();
  html += "</select>";
  html += "<button type='submit'>Apply profile</button>";
  html += "</form>";

  html += "<form method='GET' action='/profile_delete'>";
  html += "<select name='name'>";
  html += profileListOptions();
  html += "</select>";
  html += "<button class='off' type='submit'>Delete profile</button>";
  html += "</form>";
  html += "</div>";
}

/**
 * @brief Append Safety Summary Card.
 * @param html HTML string that receives generated markup.
 */
void appendSafetySummaryCard(String& html) {
  html += "<div class='grid'>";
  html += "<div class='metric'><div class='metric-label'>Safety min resistance</div><div class='metric-value'>";
  html += formatResistanceOhms(safetyMinOhm);
  html += "</div></div>";
  html += "<div class='metric'><div class='metric-label'>Max active bits</div><div class='metric-value'>";
  html += String(safetyMaxActiveBits);
  html += "</div></div>";
  html += "<div class='metric'><div class='metric-label'>Expert mode</div><div class='metric-value'>";
  html += safetyExpertMode ? "ON" : "OFF";
  html += "</div></div>";
  html += "<div class='metric'><div class='metric-label'>Uptime</div><div class='metric-value'>";
  html += String((millis() - bootMillis) / 1000UL);
  html += " s</div></div>";
  html += "</div>";
}

/**
 * @brief Append Live State Script.
 * @param html HTML string that receives generated markup.
 */
void appendLiveStateScript(String& html) {
  html += "<script>";
  html += "function upd(){fetch('/state').then(r=>r.text()).then(t=>{var e=document.getElementById('liveState');if(e)e.textContent=t;}).catch(()=>{});}setInterval(upd,2000);window.addEventListener('load',upd);";
  html += "</script>";
}


/**
 * @brief Append one channel bit-status LED row to a generated HTML page.
 * @param html HTML string that receives generated markup.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @param mask 16-bit resistance switch mask.
 */
void appendBitIndicator(String& html, uint8_t channelIndex, uint16_t mask) {
  html += "<div class='bits'>";

  // Visual order: bit 15 left, bit 0 right.
  for (int bit = 15; bit >= 0; bit--) {
    bool on = (mask & (uint16_t(1) << bit)) != 0;

    RuntimeResistorInfo* info = getRuntimeResistorInfoForBit(channelIndex, uint8_t(bit));

    html += "<a class='bit ";
    html += on ? "on" : "off";
    html += "' href='/toggle_bit?ch=";
    html += String(channelIndex + 1);
    html += "&bit=";
    html += String(bit);
    html += "' title='Click to ";
    html += on ? "turn OFF" : "turn ON";
    html += " bit ";
    html += String(bit);

    if (info != nullptr) {
      html += " / ";
      html += info->mosfet_name;
      html += " / ";
      html += info->nominal_resistance;
    }

    html += "'>";
    html += String(bit);
    html += "</a>";
  }

  html += "</div>";
}

