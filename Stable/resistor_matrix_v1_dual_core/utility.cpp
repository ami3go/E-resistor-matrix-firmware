/**
 * @file utility.cpp
 * @brief General utility functions for text formatting, status/error handling, runtime monitoring, and network configuration storage.
 */

#include "app.h"

// 01_utility.ino
// Split from rp2040_w5500_resistor_matrix_v1_safety_logic.ino.
// Keep all files in the same Arduino sketch folder.

// ============================================================
// Utility
// ============================================================

/**
 * @brief Ip To String.
 * @param ip Function parameter.
 * @return Result value; for bool, true means the operation succeeded.
 */
String ipToString(const IPAddress& ip) {
  return String(ip[0]) + "." +
         String(ip[1]) + "." +
         String(ip[2]) + "." +
         String(ip[3]);
}

/**
 * @brief Make Board Serial Number.
 * @return Result value; for bool, true means the operation succeeded.
 */
String makeBoardSerialNumber() {
#if defined(ARDUINO_ARCH_RP2040)
  // In Arduino-Pico core 5.x, rp2040.getChipID() returns a const char*
  // hex string derived from the external QSPI flash unique ID.
  const char* id = rp2040.getChipID();
  if (id == nullptr || id[0] == '\0') {
    return String("UNKNOWN");
  }
  return String(id);
#else
  return String("UNKNOWN");
#endif
}


/**
 * @brief Parse Ip Address Text.
 * @param text Function parameter.
 * @param out Function parameter.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool parseIpAddressText(const String& text, IPAddress& out) {
  String s = text;
  s.trim();

  int parts[4] = {0, 0, 0, 0};
  int partIndex = 0;
  int value = 0;
  bool haveDigit = false;

  for (uint16_t i = 0; i <= s.length(); i++) {
    char c = (i < s.length()) ? s[i] : '.';

    if (isdigit(c)) {
      value = value * 10 + (c - '0');
      if (value > 255) {
        return false;
      }
      haveDigit = true;
      continue;
    }

    if (c == '.') {
      if (!haveDigit || partIndex >= 4) {
        return false;
      }
      parts[partIndex++] = value;
      value = 0;
      haveDigit = false;
      continue;
    }

    return false;
  }

  if (partIndex != 4) {
    return false;
  }

  out = IPAddress(parts[0], parts[1], parts[2], parts[3]);
  return true;
}

/**
 * @brief Network Config To Text.
 * @return Result value; for bool, true means the operation succeeded.
 */
String networkConfigToText() {
  String out;
  out.reserve(160);
  out += "mode,static\n";
  out += "ip," + ipToString(DEVICE_IP) + "\n";
  out += "subnet," + ipToString(DEVICE_SUBNET) + "\n";
  out += "gateway," + ipToString(DEVICE_GATEWAY) + "\n";
  out += "dns," + ipToString(DEVICE_DNS) + "\n";
  return out;
}

/**
 * @brief Save Network Config To Little FS.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool saveNetworkConfigToLittleFS() {
  if (!littleFsReady) {
    return false;
  }

  File f = LittleFS.open(networkConfigPath(), "w");
  if (!f) {
    return false;
  }

  f.print(networkConfigToText());
  f.close();
  return true;
}

/**
 * @brief Load Network Config From Little FS.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool loadNetworkConfigFromLittleFS() {
  if (!littleFsReady) {
    return false;
  }

  String path = networkConfigPath();
  if (!LittleFS.exists(path)) {
    return false;
  }

  File f = LittleFS.open(path, "r");
  if (!f) {
    return false;
  }

  IPAddress newIp = DEVICE_IP;
  IPAddress newSubnet = DEVICE_SUBNET;
  IPAddress newGateway = DEVICE_GATEWAY;
  IPAddress newDns = DEVICE_DNS;

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0 || line.startsWith("#")) {
      continue;
    }

    int comma = line.indexOf(',');
    if (comma < 0) {
      continue;
    }

    String key = line.substring(0, comma);
    String value = line.substring(comma + 1);
    key.trim();
    value.trim();
    key.toLowerCase();

    IPAddress parsed;
    if (key == "ip" && parseIpAddressText(value, parsed)) {
      newIp = parsed;
    } else if (key == "subnet" && parseIpAddressText(value, parsed)) {
      newSubnet = parsed;
    } else if (key == "gateway" && parseIpAddressText(value, parsed)) {
      newGateway = parsed;
    } else if (key == "dns" && parseIpAddressText(value, parsed)) {
      newDns = parsed;
    }
  }

  f.close();

  DEVICE_IP = newIp;
  DEVICE_SUBNET = newSubnet;
  DEVICE_GATEWAY = newGateway;
  DEVICE_DNS = newDns;

  return true;
}

/**
 * @brief Hex16.
 * @param value Function parameter.
 * @return Result value; for bool, true means the operation succeeded.
 */
String hex16(uint16_t value) {
  char buf[7];
  snprintf(buf, sizeof(buf), "0x%04X", value);
  return String(buf);
}

/**
 * @brief Set Status.
 * @param text Function parameter.
 */
void setStatus(const char* text) {
  strncpy(statusText, text, sizeof(statusText) - 1);
  statusText[sizeof(statusText) - 1] = '\0';
}

/**
 * @brief Set Last Error.
 * @param text Function parameter.
 */
void setLastError(const char* text) {
  strncpy(lastError, text, sizeof(lastError) - 1);
  lastError[sizeof(lastError) - 1] = '\0';
}

/**
 * @brief Clear Last Error.
 */
void clearLastError() {
  setLastError("0,\"No error\"");
}

/**
 * @brief Send No Cache Headers.
 */
void sendNoCacheHeaders() {
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");
}

/**
 * @brief Redirect To Root.
 */
void redirectToRoot() {
  server.sendHeader("Location", "/", true);
  server.send(303, "text/plain", "See Other\n");
}

/**
 * @brief Redirect To Settings.
 */
void redirectToSettings() {
  server.sendHeader("Location", "/settings", true);
  server.send(303, "text/plain", "See Other\n");
}

/**
 * @brief Enter a firmware safe state and request all outputs OFF.
 * @param reason Output buffer for a human-readable diagnostic message.
 */
void safeState(const char* reason) {
  Serial.print("SAFE STATE: ");
  Serial.println(reason);
  Serial.flush();

  // Safety-critical behavior: a fault must physically drive every channel OFF,
  // not only update the UI/status LED. This keeps the Arduino prototype closer
  // to the production firmware safety principle.
  fatalSafeStateActive = true;

  if (shiftRegistersReady) {
    forceAllOff();
  } else {
    outputsKnownSafe = false;
    Serial.println("SAFE STATE WARNING: shift-register GPIOs not ready yet");
    Serial.flush();
  }

  setStatus(reason);
  setLastError(reason);
  appendLogEvent(reason);
  ledMode = LED_FAULT;
}

/**
 * @brief Note Http Request.
 */
void noteHttpRequest() {
  httpRequestCount++;
}

/**
 * @brief Note Scpi Command.
 */
void noteScpiCommand() {
  scpiCommandCount++;
}

/**
 * @brief Runtime Monitor Begin.
 */
void runtimeMonitorBegin() {
  runtimeWindowStartMs = millis();
  runtimeBusyAccumUs = 0;
  runtimeLoopCountWindow = 0;
  runtimeLoopMaxUs = 0;
  runtimeCore0LoadPct = 0.0f;
  runtimeLoopsPerSecond = 0.0f;
}

/**
 * @brief Runtime Monitor Update.
 * @param busyUs Function parameter.
 */
void runtimeMonitorUpdate(uint32_t busyUs) {
  runtimeBusyAccumUs += busyUs;
  runtimeLoopCountWindow++;

  if (busyUs > runtimeLoopMaxUs) {
    runtimeLoopMaxUs = busyUs;
  }

  uint32_t nowMs = millis();
  uint32_t elapsedMs = nowMs - runtimeWindowStartMs;

  if (elapsedMs >= 1000U) {
    double elapsedUs = double(elapsedMs) * 1000.0;
    double loadPct = 0.0;

    if (elapsedUs > 0.0) {
      loadPct = (double(runtimeBusyAccumUs) * 100.0) / elapsedUs;
    }

    if (loadPct < 0.0) {
      loadPct = 0.0;
    }
    if (loadPct > 100.0) {
      loadPct = 100.0;
    }

    runtimeCore0LoadPct = float(loadPct);
    runtimeLoopsPerSecond = (elapsedMs > 0U)
                              ? (float(runtimeLoopCountWindow) * 1000.0f / float(elapsedMs))
                              : 0.0f;

    runtimeBusyAccumUs = 0;
    runtimeLoopCountWindow = 0;
    runtimeLoopMaxUs = 0;
    runtimeWindowStartMs = nowMs;
  }
}

/**
 * @brief Get Heap Total Bytes.
 * @return Result value; for bool, true means the operation succeeded.
 */
uint32_t getHeapTotalBytes() {
  return rp2040.getTotalHeap();
}

/**
 * @brief Get Heap Used Bytes.
 * @return Result value; for bool, true means the operation succeeded.
 */
uint32_t getHeapUsedBytes() {
  return rp2040.getUsedHeap();
}

/**
 * @brief Get Heap Free Bytes.
 * @return Result value; for bool, true means the operation succeeded.
 */
uint32_t getHeapFreeBytes() {
  return rp2040.getFreeHeap();
}

/**
 * @brief Get Heap Used Percent.
 * @return Result value; for bool, true means the operation succeeded.
 */
float getHeapUsedPercent() {
  uint32_t total = getHeapTotalBytes();
  if (total == 0U) {
    return 0.0f;
  }
  return (float(getHeapUsedBytes()) * 100.0f) / float(total);
}

/**
 * @brief Append Runtime Monitor Info.
 * @param html HTML string that receives generated markup.
 */
void appendRuntimeMonitorInfo(String& html) {
  html += "<h2>Runtime / Memory</h2>";
  html += "<table>";
  html += "<tr><th>Item</th><th>Value</th></tr>";

  html += "<tr><td>CPU frequency</td><td><code>";
  html += String(F_CPU / 1000000UL);
  html += " MHz</code></td></tr>";

  html += "<tr><td>Heap total</td><td><code>";
  html += formatBytesHuman(getHeapTotalBytes());
  html += "</code></td></tr>";

  html += "<tr><td>Heap used</td><td><code>";
  html += formatBytesHuman(getHeapUsedBytes());
  html += "</code></td></tr>";

  html += "<tr><td>Heap free</td><td><code>";
  html += formatBytesHuman(getHeapFreeBytes());
  html += "</code></td></tr>";

  html += "<tr><td>Heap used %</td><td><code>";
  html += String(getHeapUsedPercent(), 1);
  html += "%</code></td></tr>";

  html += "<tr><td>Core 0 load estimate</td><td><code>";
  html += String(runtimeCore0LoadPct, 1);
  html += "%</code></td></tr>";

  html += "<tr><td>Loop rate</td><td><code>";
  html += String(runtimeLoopsPerSecond, 1);
  html += " loops/s</code></td></tr>";

  html += "<tr><td>Max loop busy time</td><td><code>";
  html += String(runtimeLoopMaxUs);
  html += " us</code></td></tr>";

  html += "<tr><td>HTTP request count</td><td><code>";
  html += String(httpRequestCount);
  html += "</code></td></tr>";

  html += "<tr><td>SCPI command count</td><td><code>";
  html += String(scpiCommandCount);
  html += "</code></td></tr>";

  html += "<tr><td>SCPI client state</td><td><code>";
  html += (scpiClient && scpiClient.connected()) ? "connected" : "not connected";
  html += "</code></td></tr>";

  html += "<tr><td>Core 1 state</td><td><code>";
  html += core1EngineReady ? "ready" : "not ready";
  html += "</code></td></tr>";

  html += "<tr><td>Core 1 heartbeat</td><td><code>";
  html += String((uint32_t)core1HeartbeatMs);
  html += " ms</code></td></tr>";

  html += "<tr><td>Core 1 commands</td><td><code>";
  html += String((uint32_t)core1CommandCounter);
  html += "</code></td></tr>";

  html += "<tr><td>Core 1 queue overflows</td><td><code>";
  html += String((uint32_t)core1QueueOverflowCounter);
  html += "</code></td></tr>";

  html += "</table>";

  html += "<p class='small'>Core load is an estimated firmware busy-time percentage measured over the main loop window. It is not a hardware CPU profiler.</p>";
}


/**
 * @brief Return firmware version/build string for UI and SCPI output.
 */
String firmwareVersionString() {
  String s;
  s.reserve(64);
  s += FIRMWARE_VERSION;
  s += " (";
  s += FIRMWARE_BUILD_DATE;
  s += " ";
  s += FIRMWARE_BUILD_TIME;
  s += ")";
  return s;
}

/**
 * @brief Return a single-line firmware identity string.
 */
String firmwareIdentityString() {
  String s;
  s.reserve(96);
  s += FIRMWARE_VENDOR;
  s += ",";
  s += FIRMWARE_NAME;
  s += ",";
  s += deviceSerialNumber;
  s += ",";
  s += FIRMWARE_VERSION;
  return s;
}
