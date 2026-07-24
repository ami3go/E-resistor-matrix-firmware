/** @file http_control.cpp @brief Gate 5 split HTTP service module. */
#include "http_api.h"


void handleToggleBit() {
  noteHttpRequest();

  uint8_t ch = 0;
  uint8_t bit = 0;

  if (!parseChannel(ch)) {
    server.send(400, "text/plain", "Bad channel. Use ch=1..8\n");
    return;
  }

  if (!parseBit(bit)) {
    server.send(400, "text/plain", "Bad bit. Use bit=0..15\n");
    return;
  }

  uint16_t oldMask = channelMask[ch];
  uint16_t newMask = oldMask ^ (uint16_t(1) << bit);

  char safetyReason[128];
  if (!checkMaskSafety(ch, newMask, safetyReason, sizeof(safetyReason))) {
    setLedMode(LED_FAULT);
    setLastError(safetyReason);
    server.send(400, "text/plain", String(safetyReason) + "\n");
    return;
  }

  bool ok = applyChannelMask(ch, newMask);

  if (!ok) {
    setLedMode(LED_FAULT);
    server.send(500, "text/plain", "Bit toggle apply failed\n");
    return;
  }

  char msg[128];
  snprintf(
    msg,
    sizeof(msg),
    "CH%u bit %u toggled %s: 0x%04X -> 0x%04X",
    unsigned(ch + 1),
    unsigned(bit),
    (newMask & (uint16_t(1) << bit)) ? "ON" : "OFF",
    unsigned(oldMask),
    unsigned(newMask)
  );

  setStatus(msg);
  appendLogEvent(msg);
  clearLastError();
  setLedMode(LED_OK);

  redirectToRoot();
}

void handleTargetApply() {
  noteHttpRequest();

  uint8_t ch = 0;
  if (!parseChannel(ch)) {
    server.send(400, "text/plain", "Bad channel. Use ch=1..8\n");
    return;
  }

  if (!server.hasArg("target")) {
    server.send(400, "text/plain", "Missing target resistance in ohms\n");
    return;
  }

  double targetOhm = server.arg("target").toFloat();
  if (targetOhm <= 0.0) {
    server.send(400, "text/plain", "Bad target resistance\n");
    return;
  }

  uint16_t mask = 0;
  double calcOhm = 0.0;
  double errPct = 0.0;

  if (!findNearestMaskForTarget(ch, targetOhm, mask, calcOhm, errPct)) {
    server.send(400, "text/plain", "No safe mask found for target\n");
    return;
  }

  char reason[128];
  if (!checkMaskSafety(ch, mask, reason, sizeof(reason))) {
    server.send(400, "text/plain", String(reason) + "\n");
    return;
  }

  if (!applyChannelMask(ch, mask)) {
    server.send(500, "text/plain", "Apply failed\n");
    return;
  }

  char msg[128];
  snprintf(msg, sizeof(msg), "CH%u target %.3f Ohm -> 0x%04X, calc %.3f Ohm, err %.4f%%",
           unsigned(ch + 1), targetOhm, unsigned(mask), calcOhm, errPct);
  setStatus(msg);
  appendLogEvent(msg);
  clearLastError();

  redirectToRoot();
}

void handleIdentifyLed() {
  noteHttpRequest();

  startIdentifyLedBlink(5000UL);

  setStatus("Identify LED: bright blue blink for 5 seconds");
  appendLogEvent("Identify LED requested: blue blink for 5 seconds");
  clearLastError();

  redirectToRoot();
}

void handleProfileSave() {
  noteHttpRequest();

  String name = server.hasArg("name") ? server.arg("name") : "profile";
  bool ok = saveCurrentProfile(name);

  if (ok) {
    String msg = String("Profile saved: ") + sanitizeName(name);
    setStatus(msg.c_str());
    appendLogEvent(msg.c_str());
    clearLastError();
  } else {
    setStatus("Profile save failed");
    setLastError("Profile save failed");
  }

  server.sendHeader("Location", "/profiles", true);
  server.send(303, "text/plain", "See Other\n");
}

void handleProfileApply() {
  noteHttpRequest();

  if (!server.hasArg("name")) {
    server.send(400, "text/plain", "Missing profile name\n");
    return;
  }

  uint16_t masks[CHANNEL_COUNT];
  if (!loadProfileMasks(server.arg("name"), masks)) {
    server.send(404, "text/plain", "Profile not found\n");
    return;
  }

  char reason[128];
  if (!applyAllMasksSafely(masks, reason, sizeof(reason))) {
    server.send(400, "text/plain", String("Profile rejected/apply failed: ") + reason + "\n");
    return;
  }

  String msg = String("Profile applied: ") + sanitizeName(server.arg("name"));
  setStatus(msg.c_str());
  appendLogEvent(msg.c_str());
  clearLastError();

  server.sendHeader("Location", "/", true);
  server.send(303, "text/plain", "See Other\n");
}

void handleProfileDelete() {
  noteHttpRequest();

  if (!server.hasArg("name")) {
    server.send(400, "text/plain", "Missing profile name\n");
    return;
  }

  String path = profilePathFromName(server.arg("name"));
  bool ok = littleFsReady && LittleFS.exists(path) && LittleFS.remove(path);

  String msg = ok ? String("Profile deleted: ") + sanitizeName(server.arg("name")) : "Profile delete failed";
  setStatus(msg.c_str());
  appendLogEvent(msg.c_str());

  server.sendHeader("Location", "/profiles", true);
  server.send(303, "text/plain", "See Other\n");
}

void handleSafetySave() {
  noteHttpRequest();

  if (!httpConfigurationOutputsAreOff()) {
    server.send(409, "text/plain", "Turn all channels OFF before changing safety policy.\n");
    return;
  }

  double previousMin[CHANNEL_COUNT];
  double previousMax[CHANNEL_COUNT];
  uint8_t previousBits[CHANNEL_COUNT];
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
    previousMin[ch] = safetyMinOhm[ch];
    previousMax[ch] = safetyMaxOhm[ch];
    previousBits[ch] = safetyMaxActiveBits[ch];
  }
  const bool previousExpert = safetyExpertMode;

  double newMin[CHANNEL_COUNT];
  double newMax[CHANNEL_COUNT];
  uint8_t newBits[CHANNEL_COUNT];

  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    String suffix = String(ch + 1);
    String minName = String("min_ohm_ch") + suffix;
    String maxName = String("max_ohm_ch") + suffix;
    String bitsName = String("max_bits_ch") + suffix;

    if (!server.hasArg(minName) || !server.hasArg(maxName) || !server.hasArg(bitsName)) {
      server.send(400, "text/plain", String("Missing safety value for CH") + suffix + "\n");
      return;
    }

    newMin[ch] = server.arg(minName).toFloat();
    newMax[ch] = server.arg(maxName).toFloat();
    int bits = server.arg(bitsName).toInt();

    if (newMin[ch] <= 0.0 || newMax[ch] <= 0.0 || newMin[ch] > newMax[ch]) {
      server.send(400, "text/plain", String("Invalid resistance range for CH") + suffix + "\n");
      return;
    }
    if (bits < 1 || bits > BIT_COUNT) {
      server.send(400, "text/plain", String("Invalid maximum active bits for CH") + suffix + "\n");
      return;
    }
    newBits[ch] = uint8_t(bits);
  }

  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    safetyMinOhm[ch] = newMin[ch];
    safetyMaxOhm[ch] = newMax[ch];
    safetyMaxActiveBits[ch] = newBits[ch];
  }
  safetyExpertMode = server.hasArg("expert") && server.arg("expert") == "1";

  char policyReason[128] = {0};
  if (!installCore1PolicySnapshot(policyReason, sizeof(policyReason))) {
    for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
      safetyMinOhm[ch] = previousMin[ch];
      safetyMaxOhm[ch] = previousMax[ch];
      safetyMaxActiveBits[ch] = previousBits[ch];
    }
    safetyExpertMode = previousExpert;
    installCore1PolicySnapshot(nullptr, 0);
    setLastError(policyReason);
    server.send(500, "text/plain", String("Core 1 policy installation failed: ") + policyReason + "\n");
    return;
  }

  bool saved = saveSafetyConfigToLittleFS();
  setStatus(saved ? "Per-channel safety settings saved" : "Per-channel safety settings changed in RAM only");
  appendLogEvent("Per-channel safety settings updated");

  server.sendHeader("Location", "/safety", true);
  server.send(303, "text/plain", "See Other\n");
}

void handleSet() {
  noteHttpRequest();

  Serial.println("HTTP /set begin");
  Serial.flush();

  setLedMode(LED_ACTIVITY);

  uint8_t ch = 0;
  uint16_t mask = 0;

  if (!parseChannel(ch)) {
    setLedMode(LED_FAULT);
    server.send(400, "text/plain", "Bad channel. Use ch=1..8\n");
    return;
  }

  if (!parseMask(mask)) {
    setLedMode(LED_FAULT);
    server.send(400, "text/plain", "Bad mask. Use mask=0000..FFFF or 0x0000..0xFFFF\n");
    return;
  }

  Serial.print("Parsed CH");
  Serial.print(ch + 1);
  Serial.print(" mask=");
  Serial.println(hex16(mask));
  Serial.flush();

  char safetyReason[128];
  if (!checkMaskSafety(ch, mask, safetyReason, sizeof(safetyReason))) {
    setLedMode(LED_FAULT);
    setLastError(safetyReason);
    server.send(400, "text/plain", String(safetyReason) + "\n");
    return;
  }

  bool ok = applyChannelMask(ch, mask);

  if (!ok) {
    setLedMode(LED_FAULT);
    server.send(500, "text/plain", "Apply failed\n");
    return;
  }

  char msg[96];
  snprintf(
    msg,
    sizeof(msg),
    "CH%u set to 0x%04X",
    unsigned(ch + 1),
    unsigned(mask)
  );

  setStatus(msg);
  appendLogEvent(msg);
  clearLastError();
  setLedMode(LED_OK);

  redirectToRoot();
}

void handleAllOff() {
  noteHttpRequest();

  Serial.println("HTTP /alloff");
  Serial.flush();

  char reason[128] = {0};
  if (!forceAllOff(reason, sizeof(reason))) {
    char msg[180];
    snprintf(msg, sizeof(msg), "ALL OFF by HTTP failed: %s", reason[0] ? reason : "unknown failure");
    appendLogEvent(msg);
    server.send(500, "text/plain", String(msg) + "\n");
    return;
  }

  appendLogEvent("ALL OFF by HTTP");
  clearLastError();
  redirectToRoot();
}


// Canonical Gate 5 API v1 mutation endpoints. The existing handlers remain the
// single execution path so safety validation and Core 1 submission cannot drift.
void handleApiV1ChannelSet() { ++httpApiV1RequestCount; handleSet(); }
void handleApiV1AllOff() { ++httpApiV1RequestCount; handleAllOff(); }
void handleApiV1ProfileApply() { ++httpApiV1RequestCount; handleProfileApply(); }
void handleApiV1TargetApply() { ++httpApiV1RequestCount; handleTargetApply(); }
void handleApiV1ToggleBit() { ++httpApiV1RequestCount; handleToggleBit(); }
void handleApiV1IdentifyLed() { ++httpApiV1RequestCount; handleIdentifyLed(); }
