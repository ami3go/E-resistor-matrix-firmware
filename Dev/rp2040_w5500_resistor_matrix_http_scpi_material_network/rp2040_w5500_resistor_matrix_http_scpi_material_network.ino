/*
  RP2040-Zero + W5500 + 8-channel 74HC595 resistor matrix
  Static direct HTTP control + SCPI TCP server + per-channel runtime resistor config

  Arduino core:
    Earle Philhower Raspberry Pi Pico/RP2040 core

  Required library:
    Adafruit NeoPixel

  Optional but recommended:
    Enable a LittleFS filesystem in Arduino board menu if available.
    If LittleFS is unavailable, uploaded channel configs work only until reboot.

  PC Ethernet manual config:
    IP:      192.168.7.10
    Subnet:  255.255.255.0
    Gateway: empty
    DNS:     empty

  Device:
    HTTP: http://192.168.7.50/
    SCPI: TCP port 5025

  W5500:
    MISO GP0
    CS   GP1
    SCK  GP2
    MOSI GP3
    INT  GP4 unused
    RST  not connected

  Shift registers:
    DATA  GP11
    CLOCK GP12
    SRCLR GP15 active low, external 10k pull-up to 3V3

  Latch pins:
    CH1 GP5
    CH2 GP6
    CH3 GP7
    CH4 GP8
    CH5 GP9
    CH6 GP10
    CH7 GP13
    CH8 GP14

  WS2812 heartbeat:
    GP16

  Mask behavior:
    bit 0  -> Q16, lowest resistance branch
    bit 15 -> Q1, highest resistance branch

    Firmware shifts bit 0 first.
    Firmware shifts bit 15 last.

  Safety:
    Logic 1 = MOSFET ON
    Logic 0 = MOSFET OFF

    Channel update:
      1. Latch 0x0000 to selected channel
      2. Wait break-before-make delay
      3. Shift new mask
      4. Pulse selected latch
      5. Update shadow state after latch
*/

#include <Arduino.h>
#include <SPI.h>
#include <W5500lwIP.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <LittleFS.h>
#include <ctype.h>
#include <stdlib.h>

#include "board_config.h"

// ============================================================
// Ethernet pins
// ============================================================

static constexpr uint8_t ETH_MISO = 0;
static constexpr uint8_t ETH_CS   = 1;
static constexpr uint8_t ETH_SCK  = 2;
static constexpr uint8_t ETH_MOSI = 3;
static constexpr uint8_t ETH_INT  = 4;

// ============================================================
// Shift-register pins
// ============================================================

static constexpr uint8_t SR_DATA  = 11;
static constexpr uint8_t SR_CLOCK = 12;
static constexpr uint8_t SR_RESET = 15;  // active low

static constexpr uint8_t CHANNEL_COUNT = 8;
static constexpr uint8_t BIT_COUNT = 16;

static constexpr uint8_t SR_LATCH_PINS[CHANNEL_COUNT] = {
  5,   // CH1
  6,   // CH2
  7,   // CH3
  8,   // CH4
  9,   // CH5
  10,  // CH6
  13,  // CH7
  14   // CH8
};

// ============================================================
// WS2812 heartbeat
// ============================================================

static constexpr uint8_t WS2812_PIN = 16;
static constexpr uint8_t WS2812_COUNT = 1;

Adafruit_NeoPixel heartbeatPixel(
  WS2812_COUNT,
  WS2812_PIN,
  NEO_GRB + NEO_KHZ800
);

enum LedMode : uint8_t {
  LED_BOOT,
  LED_OK,
  LED_ACTIVITY,
  LED_FAULT
};

static LedMode ledMode = LED_BOOT;
static bool ledPhase = false;
static uint32_t lastLedUpdateMs = 0;

// ============================================================
// Timing
// ============================================================

static constexpr uint32_t ETH_INIT_SPI_HZ    = 1000000UL;
static constexpr uint32_t ETH_RUNTIME_SPI_HZ = 4000000UL;  // keep 4 MHz while debugging

static constexpr uint16_t SR_CLOCK_HALF_PERIOD_US = 10;
static constexpr uint16_t SR_LATCH_PULSE_US       = 10;
static constexpr uint16_t BREAK_BEFORE_MAKE_MS    = 2;

// ============================================================
// Static direct IP
// ============================================================

static IPAddress DEVICE_IP(192, 168, 7, 50);
static IPAddress DEVICE_DNS(0, 0, 0, 0);
static IPAddress DEVICE_GATEWAY(0, 0, 0, 0);
static IPAddress DEVICE_SUBNET(255, 255, 255, 0);

// ============================================================
// Ethernet / HTTP / SCPI
// ============================================================

static constexpr uint16_t HTTP_TCP_PORT = 80;
static constexpr uint16_t SCPI_TCP_PORT = 5025;

Wiznet5500lwIP eth(ETH_CS);
WebServer server(HTTP_TCP_PORT);
WiFiServer scpiServer(SCPI_TCP_PORT);
WiFiClient scpiClient;

static char scpiLine[160];
static size_t scpiLineLen = 0;

// ============================================================
// Runtime resistor table
// ============================================================

struct RuntimeResistorInfo {
  uint8_t bit;
  char mosfet_name[8];
  char nominal_resistance[16];
};

static RuntimeResistorInfo channelResistorTable[CHANNEL_COUNT][BIT_COUNT];

// ============================================================
// State
// ============================================================

static bool ethernetFault = false;
static bool littleFsReady = false;
static uint8_t w5500Version = 0;

static uint16_t channelMask[CHANNEL_COUNT] = {
  0, 0, 0, 0, 0, 0, 0, 0
};

static uint32_t applyCounter[CHANNEL_COUNT] = {
  0, 0, 0, 0, 0, 0, 0, 0
};

static char statusText[128] = "Boot";
static char lastError[128] = "0,\"No error\"";

// ============================================================
// Runtime monitor
// ============================================================

static uint32_t runtimeWindowStartMs = 0;
static uint64_t runtimeBusyAccumUs = 0;
static uint32_t runtimeLoopCountWindow = 0;
static uint32_t runtimeLoopMaxUs = 0;
static float runtimeCore0LoadPct = 0.0f;
static float runtimeLoopsPerSecond = 0.0f;
static uint32_t httpRequestCount = 0;
static uint32_t scpiCommandCount = 0;

String formatBytesHuman(uint64_t bytes);
String networkConfigPath();

// ============================================================
// Utility
// ============================================================

String ipToString(const IPAddress& ip) {
  return String(ip[0]) + "." +
         String(ip[1]) + "." +
         String(ip[2]) + "." +
         String(ip[3]);
}


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

String hex16(uint16_t value) {
  char buf[7];
  snprintf(buf, sizeof(buf), "0x%04X", value);
  return String(buf);
}

void setStatus(const char* text) {
  strncpy(statusText, text, sizeof(statusText) - 1);
  statusText[sizeof(statusText) - 1] = '\0';
}

void setLastError(const char* text) {
  strncpy(lastError, text, sizeof(lastError) - 1);
  lastError[sizeof(lastError) - 1] = '\0';
}

void clearLastError() {
  setLastError("0,\"No error\"");
}

void sendNoCacheHeaders() {
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");
}

void redirectToRoot() {
  server.sendHeader("Location", "/", true);
  server.send(303, "text/plain", "See Other\n");
}

void redirectToSettings() {
  server.sendHeader("Location", "/settings", true);
  server.send(303, "text/plain", "See Other\n");
}

void safeState(const char* reason) {
  Serial.print("SAFE STATE: ");
  Serial.println(reason);
  Serial.flush();

  setStatus(reason);
  setLastError(reason);
  ledMode = LED_FAULT;
}

void noteHttpRequest() {
  httpRequestCount++;
}

void noteScpiCommand() {
  scpiCommandCount++;
}

void runtimeMonitorBegin() {
  runtimeWindowStartMs = millis();
  runtimeBusyAccumUs = 0;
  runtimeLoopCountWindow = 0;
  runtimeLoopMaxUs = 0;
  runtimeCore0LoadPct = 0.0f;
  runtimeLoopsPerSecond = 0.0f;
}

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

uint32_t getHeapTotalBytes() {
  return rp2040.getTotalHeap();
}

uint32_t getHeapUsedBytes() {
  return rp2040.getUsedHeap();
}

uint32_t getHeapFreeBytes() {
  return rp2040.getFreeHeap();
}

float getHeapUsedPercent() {
  uint32_t total = getHeapTotalBytes();
  if (total == 0U) {
    return 0.0f;
  }
  return (float(getHeapUsedBytes()) * 100.0f) / float(total);
}

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

  html += "<tr><td>Core 1 state</td><td><code>unused (no setup1()/loop1())</code></td></tr>";

  html += "</table>";

  html += "<p class='small'>Core load is an estimated firmware busy-time percentage measured over the main loop window. It is not a hardware CPU profiler.</p>";
}

// ============================================================
// Runtime resistor configuration
// ============================================================

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

String channelConfigPath(uint8_t channelIndex) {
  return "/ch" + String(channelIndex + 1) + ".csv";
}

String networkConfigPath() {
  return "/network.csv";
}

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

// ============================================================
// Resistance calculation helpers
// ============================================================

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

void appendBitIndicator(String& html, uint8_t channelIndex, uint16_t mask) {
  html += "<div class='bits'>";

  // Visual order: bit 15 left, bit 0 right.
  for (int bit = 15; bit >= 0; bit--) {
    bool on = (mask & (uint16_t(1) << bit)) != 0;

    RuntimeResistorInfo* info = getRuntimeResistorInfoForBit(channelIndex, uint8_t(bit));

    html += "<span class='bit ";
    html += on ? "on" : "off";
    html += "' title='bit ";
    html += String(bit);

    if (info != nullptr) {
      html += " / ";
      html += info->mosfet_name;
      html += " / ";
      html += info->nominal_resistance;
    }

    html += "'>";
    html += String(bit);
    html += "</span>";
  }

  html += "</div>";
}

// ============================================================
// WS2812 heartbeat
// ============================================================

void ws2812Set(uint8_t r, uint8_t g, uint8_t b) {
  heartbeatPixel.setPixelColor(0, heartbeatPixel.Color(r, g, b));
  heartbeatPixel.show();
}

void heartbeatBegin() {
  heartbeatPixel.begin();
  heartbeatPixel.setBrightness(24);
  ws2812Set(0, 0, 8);
}

void setLedMode(LedMode mode) {
  ledMode = mode;
  lastLedUpdateMs = 0;
}

void updateHeartbeat() {
  uint32_t now = millis();

  uint16_t intervalMs = 700;

  if (ledMode == LED_ACTIVITY) {
    intervalMs = 120;
  } else if (ledMode == LED_FAULT) {
    intervalMs = 250;
  } else if (ledMode == LED_BOOT) {
    intervalMs = 400;
  }

  if (now - lastLedUpdateMs < intervalMs) {
    return;
  }

  lastLedUpdateMs = now;
  ledPhase = !ledPhase;

  if (!ledPhase) {
    ws2812Set(0, 0, 0);
    return;
  }

  switch (ledMode) {
    case LED_BOOT:
      ws2812Set(0, 0, 16);
      break;

    case LED_OK:
      ws2812Set(0, 18, 0);
      break;

    case LED_ACTIVITY:
      ws2812Set(18, 12, 0);
      break;

    case LED_FAULT:
    default:
      ws2812Set(24, 0, 0);
      break;
  }
}

// ============================================================
// W5500 low-level register access
// ============================================================

void w5500WriteCommonReg(uint16_t address, uint8_t value, uint32_t spiHz) {
  SPI.beginTransaction(SPISettings(spiHz, MSBFIRST, SPI_MODE0));

  digitalWrite(ETH_CS, LOW);

  SPI.transfer((address >> 8) & 0xFF);
  SPI.transfer(address & 0xFF);
  SPI.transfer(0x04);
  SPI.transfer(value);

  digitalWrite(ETH_CS, HIGH);

  SPI.endTransaction();
}

uint8_t w5500ReadCommonReg(uint16_t address, uint32_t spiHz) {
  SPI.beginTransaction(SPISettings(spiHz, MSBFIRST, SPI_MODE0));

  digitalWrite(ETH_CS, LOW);

  SPI.transfer((address >> 8) & 0xFF);
  SPI.transfer(address & 0xFF);
  SPI.transfer(0x00);
  uint8_t value = SPI.transfer(0x00);

  digitalWrite(ETH_CS, HIGH);

  SPI.endTransaction();

  return value;
}

bool w5500SoftwareResetAndProbe() {
  Serial.println("W5500: software reset");
  Serial.flush();

  w5500WriteCommonReg(0x0000, 0x80, ETH_INIT_SPI_HZ);
  delay(20);

  w5500Version = w5500ReadCommonReg(0x0039, ETH_INIT_SPI_HZ);

  Serial.print("W5500 VERSIONR = 0x");
  Serial.println(w5500Version, HEX);
  Serial.flush();

  if (w5500Version != 0x04) {
    ethernetFault = true;
    safeState("W5500 VERSION check failed");
    return false;
  }

  uint8_t phy = w5500ReadCommonReg(0x002E, ETH_INIT_SPI_HZ);

  Serial.print("W5500 PHYCFGR = 0x");
  Serial.println(phy, HEX);

  bool linkUp = phy & 0x01;
  bool speed100 = phy & 0x02;
  bool fullDuplex = phy & 0x04;

  Serial.print("Ethernet link: ");
  Serial.println(linkUp ? "UP" : "DOWN");

  Serial.print("Speed: ");
  Serial.println(speed100 ? "100M" : "10M");

  Serial.print("Duplex: ");
  Serial.println(fullDuplex ? "FULL" : "HALF");
  Serial.flush();

  if (!linkUp) {
    ethernetFault = true;
    safeState("Ethernet link DOWN");
    return false;
  }

  return true;
}

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

bool applyChannelMask(uint8_t channelIndex, uint16_t newMask) {
  Serial.println("applyChannelMask: start");
  Serial.flush();

  if (channelIndex >= CHANNEL_COUNT) {
    Serial.println("applyChannelMask: bad channel");
    Serial.flush();
    return false;
  }

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

  Serial.println("applyChannelMask: done");
  Serial.flush();

  return true;
}

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
}

void forceAllOff() {
  Serial.println("Shift registers: all OFF start");
  Serial.flush();

  setLedMode(LED_ACTIVITY);

  digitalWrite(SR_DATA, LOW);
  digitalWrite(SR_CLOCK, LOW);

  // Keep SRCLR inactive during debug.
  digitalWrite(SR_RESET, HIGH);

  Serial.println("  basic SR pins set");
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

  setStatus("All channels OFF");
  setLedMode(LED_OK);

  Serial.println("Shift registers: all OFF done");
  Serial.flush();
}

// ============================================================
// Ethernet startup
// ============================================================

bool startEthernetStatic() {
  Serial.println();
  Serial.println("Ethernet: static direct mode");
  Serial.flush();

  lwipPollingPeriod(20);
  eth.setSPISpeed(ETH_RUNTIME_SPI_HZ);

  eth.config(
    DEVICE_IP,
    DEVICE_DNS,
    DEVICE_GATEWAY,
    DEVICE_SUBNET
  );

  eth.begin();

  uint32_t start = millis();

  while (millis() - start < 10000) {
    updateHeartbeat();

    IPAddress ip = eth.localIP();

    Serial.print("connected=");
    Serial.print(eth.connected());
    Serial.print(" ip=");
    Serial.println(ipToString(ip));
    Serial.flush();

    if (ip == DEVICE_IP) {
      Serial.println("Ethernet static IP OK");
      Serial.flush();

      setStatus("Ethernet OK");
      return true;
    }

    delay(500);
  }

  ethernetFault = true;
  safeState("Ethernet static IP failed");
  return false;
}

// ============================================================
// HTTP / SCPI parsing helpers
// ============================================================

bool parseChannel(uint8_t& channelIndex) {
  if (!server.hasArg("ch")) {
    return false;
  }

  int ch = server.arg("ch").toInt();

  if (ch < 1 || ch > 8) {
    return false;
  }

  channelIndex = uint8_t(ch - 1);
  return true;
}

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

bool parseMask(uint16_t& mask) {
  if (!server.hasArg("mask")) {
    return false;
  }

  return parseHex16String(server.arg("mask"), mask);
}

// ============================================================
// HTTP page helpers
// ============================================================

void appendCommonPageHeader(String& html, const char* title) {
  html += "<!doctype html><html><head>";
  html += "<meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<meta http-equiv='Cache-Control' content='no-store'>";
  html += "<title>";
  html += title;
  html += "</title>";

  html += "<style>";
  html += ":root{--bg:#0f1117;--surface:#171c24;--surface2:#202733;--surface3:#273041;--primary:#5b8cff;--primary2:#7ca6ff;--danger:#ff5d5d;--ok:#35d07f;--warn:#ffd166;--text:#eef3fb;--muted:#a9b3c3;--border:#303a49;--shadow:0 12px 30px rgba(0,0,0,.28);--radius:16px;}";
  html += "*{box-sizing:border-box;}";
  html += "body{margin:0;font-family:Arial,Helvetica,sans-serif;background:linear-gradient(135deg,#0f1117,#141b28 55%,#10141c);color:var(--text);}";
  html += ".topbar{position:sticky;top:0;z-index:10;background:rgba(15,17,23,.92);backdrop-filter:blur(10px);border-bottom:1px solid var(--border);box-shadow:0 4px 18px rgba(0,0,0,.22);}";
  html += ".topbar-inner{max-width:1600px;margin:0 auto;padding:14px 18px;display:flex;gap:16px;align-items:center;justify-content:space-between;flex-wrap:wrap;}";
  html += ".brand{font-size:18px;font-weight:800;letter-spacing:.2px;display:flex;gap:10px;align-items:center;}";
  html += ".dot{width:12px;height:12px;border-radius:50%;background:var(--ok);box-shadow:0 0 14px var(--ok);display:inline-block;}";
  html += ".tabs{display:flex;gap:8px;flex-wrap:wrap;}";
  html += ".tab{color:var(--text);text-decoration:none;padding:9px 13px;border-radius:999px;border:1px solid var(--border);background:var(--surface);font-size:14px;font-weight:600;}";
  html += ".tab:hover{background:var(--surface3);border-color:var(--primary);}";
  html += ".page{max-width:1600px;margin:0 auto;padding:22px 18px 40px;}";
  html += "h1{font-size:30px;margin:10px 0 8px;}h2{font-size:21px;margin:26px 0 12px;}h3{font-size:17px;margin:20px 0 10px;}";
  html += ".card{background:linear-gradient(180deg,var(--surface),#141922);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin:16px 0;box-shadow:var(--shadow);}";
  html += ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:16px;}";
  html += ".metric{background:var(--surface2);border:1px solid var(--border);border-radius:14px;padding:14px;}";
  html += ".metric-label{font-size:12px;color:var(--muted);text-transform:uppercase;letter-spacing:.06em;}";
  html += ".metric-value{font-size:22px;font-weight:800;margin-top:4px;}";
  html += "input,button,select,textarea{font-size:16px;padding:10px 12px;margin:4px;border-radius:12px;border:1px solid var(--border);}";
  html += "input,select,textarea{background:var(--surface2);color:var(--text);outline:none;}";
  html += "input:focus,select:focus,textarea:focus{border-color:var(--primary);box-shadow:0 0 0 3px rgba(91,140,255,.18);}";
  html += "textarea{width:100%;max-width:980px;height:360px;font-family:monospace;display:block;}";
  html += "button{background:var(--primary);color:white;font-weight:700;cursor:pointer;border-color:transparent;}button:hover{background:var(--primary2);}";
  html += "table{border-collapse:separate;border-spacing:0;margin-top:14px;width:100%;max-width:none;background:var(--surface);border:1px solid var(--border);border-radius:14px;overflow:hidden;box-shadow:0 8px 22px rgba(0,0,0,.18);}";
  html += "td,th{border-bottom:1px solid var(--border);padding:10px 12px;text-align:left;vertical-align:middle;}tr:last-child td{border-bottom:0;}th{background:var(--surface3);color:#dfe7f5;font-size:13px;text-transform:uppercase;letter-spacing:.04em;}";
  html += "code,pre{color:#a7f3d0;}pre{background:#0b0f14;border:1px solid var(--border);border-radius:12px;padding:14px;overflow:auto;}";
  html += "a{color:#93bbff;}";
  html += ".warn{color:var(--warn);}.ok{color:var(--ok);}.danger{color:var(--danger);}";
  html += ".apply{background:var(--primary);color:white;}.off{background:#722020;color:white;}.off:hover{background:#9a2929;}";
  html += ".mask{width:120px;font-family:monospace;}.table-scroll{width:100%;overflow-x:auto;}.control-table{min-width:1380px;}.control-table td:nth-child(3){min-width:535px;}.control-table td:nth-child(5){min-width:260px;white-space:nowrap;}";
  html += ".small{color:var(--muted);font-size:14px;line-height:1.45;}";
  html += ".bits{display:flex;flex-wrap:nowrap;gap:4px;min-width:492px;white-space:nowrap;}";
  html += ".bit{display:inline-block;flex:0 0 27px;width:27px;height:25px;line-height:25px;text-align:center;border:1px solid var(--border);border-radius:7px;font-size:11px;font-family:monospace;}";
  html += ".bit.on{background:var(--ok);color:#06130c;border-color:#73f0aa;box-shadow:0 0 10px rgba(53,208,127,.55);font-weight:800;}";
  html += ".bit.off{background:#111720;color:#667085;border-color:#2b3442;}";
  html += ".res{font-family:monospace;color:#a7f3d0;white-space:nowrap;font-weight:700;}";
  html += ".notice{border-left:4px solid var(--primary);padding:12px 14px;background:rgba(91,140,255,.10);border-radius:12px;margin:14px 0;}";
  html += "@media(max-width:720px){.topbar-inner{align-items:flex-start}.tabs{width:100%;}.tab{flex:1;text-align:center;}table{font-size:13px;}td,th{padding:8px;}h1{font-size:24px;}}";
  html += "</style>";

  html += "</head><body>";
  html += "<header class='topbar'><div class='topbar-inner'>";
  html += "<div class='brand'><span class='dot'></span><span>RP2040 Matrix</span></div>";
  html += "<nav class='tabs'>";
  html += "<a class='tab' href='/'>Control</a>";
  html += "<a class='tab' href='/settings'>Calibration</a>";
  html += "<a class='tab' href='/network'>Ethernet</a>";
  html += "<a class='tab' href='/runtime'>Runtime</a>";
  html += "<a class='tab' href='/files'>Files</a>";
  html += "<a class='tab' href='/state'>State</a>";
  html += "</nav>";
  html += "</div></header>";
  html += "<main class='page'>";
}

void appendCommonPageFooter(String& html) {
  html += "</main></body></html>";
}


// ============================================================
// LittleFS Settings page helpers
// ============================================================

String u64ToString(uint64_t value) {
  char buf[32];
  snprintf(buf, sizeof(buf), "%llu", (unsigned long long)value);
  return String(buf);
}

String formatBytesHuman(uint64_t bytes) {
  char buf[48];

  if (bytes >= 1024ULL * 1024ULL) {
    double mb = double(bytes) / double(1024ULL * 1024ULL);
    snprintf(buf, sizeof(buf), "%llu bytes (%.2f MiB)", (unsigned long long)bytes, mb);
  } else if (bytes >= 1024ULL) {
    double kb = double(bytes) / 1024.0;
    snprintf(buf, sizeof(buf), "%llu bytes (%.2f KiB)", (unsigned long long)bytes, kb);
  } else {
    snprintf(buf, sizeof(buf), "%llu bytes", (unsigned long long)bytes);
  }

  return String(buf);
}

void appendLittleFsChannelConfigStatus(String& html) {
  html += "<h3>Expected channel config files</h3>";
  html += "<table>";
  html += "<tr><th>Channel</th><th>File path</th><th>Status</th><th>Size</th></tr>";

  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    String path = channelConfigPath(ch);
    bool exists = littleFsReady && LittleFS.exists(path);
    uint64_t size = 0;

    if (exists) {
      File f = LittleFS.open(path, "r");
      if (f) {
        size = f.size();
        f.close();
      }
    }

    html += "<tr><td>CH";
    html += String(ch + 1);
    html += "</td><td><code>";
    html += path;
    html += "</code></td><td>";
    html += exists ? "<span class='ok'>stored</span>" : "<span class='warn'>not stored</span>";
    html += "</td><td>";
    html += exists ? formatBytesHuman(size) : "-";
    html += "</td></tr>";
  }

  html += "</table>";
}

void appendLittleFsStoredFiles(String& html) {
  html += "<h3>Stored files in LittleFS root</h3>";

  if (!littleFsReady) {
    html += "<p class='warn'>LittleFS is not ready, so stored files cannot be listed.</p>";
    return;
  }

  Dir dir = LittleFS.openDir("/");
  uint16_t fileCount = 0;
  uint64_t listedBytes = 0;

  html += "<table>";
  html += "<tr><th>#</th><th>File name</th><th>Full path</th><th>Size</th></tr>";

  while (dir.next()) {
    String name = dir.fileName();
    String fullPath = name;

    if (!fullPath.startsWith("/")) {
      fullPath = "/" + fullPath;
    }

    uint64_t size = dir.fileSize();
    listedBytes += size;
    fileCount++;

    html += "<tr><td>";
    html += String(fileCount);
    html += "</td><td><code>";
    html += name;
    html += "</code></td><td><code>";
    html += fullPath;
    html += "</code></td><td>";
    html += formatBytesHuman(size);
    html += "</td></tr>";
  }

  if (fileCount == 0) {
    html += "<tr><td colspan='4'><span class='warn'>No files stored in root directory.</span></td></tr>";
  } else {
    html += "<tr><th colspan='3'>Listed files total</th><th>";
    html += formatBytesHuman(listedBytes);
    html += "</th></tr>";
  }

  html += "</table>";
}

void appendLittleFsStorageInfo(String& html) {
  html += "<h2>LittleFS storage status</h2>";

  html += "<p>LittleFS mount state: <code>";
  html += littleFsReady ? "ready" : "not ready - uploads will be RAM only until reboot";
  html += "</code></p>";

  if (!littleFsReady) {
    html += "<p class='warn'>No filesystem size or file list is available because LittleFS did not mount.</p>";
    appendLittleFsChannelConfigStatus(html);
    return;
  }

  FSInfo fsInfo;
  if (!LittleFS.info(fsInfo)) {
    html += "<p class='warn'>LittleFS.info() failed. Files may still be usable, but capacity information is unavailable.</p>";
    appendLittleFsChannelConfigStatus(html);
    appendLittleFsStoredFiles(html);
    return;
  }

  uint64_t totalBytes = fsInfo.totalBytes;
  uint64_t usedBytes = fsInfo.usedBytes;
  uint64_t freeBytes = 0;

  if (totalBytes >= usedBytes) {
    freeBytes = totalBytes - usedBytes;
  }

  double usedPercent = 0.0;
  if (totalBytes > 0) {
    usedPercent = (double(usedBytes) * 100.0) / double(totalBytes);
  }

  html += "<table>";
  html += "<tr><th>Metric</th><th>Value</th></tr>";

  html += "<tr><td>Total LittleFS size</td><td><code>";
  html += formatBytesHuman(totalBytes);
  html += "</code></td></tr>";

  html += "<tr><td>Used space</td><td><code>";
  html += formatBytesHuman(usedBytes);
  html += "</code></td></tr>";

  html += "<tr><td>Free space</td><td><code>";
  html += formatBytesHuman(freeBytes);
  html += "</code></td></tr>";

  html += "<tr><td>Used percent</td><td><code>";
  html += String(usedPercent, 2);
  html += "%</code></td></tr>";

  html += "<tr><td>Block size</td><td><code>";
  html += formatBytesHuman(fsInfo.blockSize);
  html += "</code></td></tr>";

  html += "<tr><td>Page size</td><td><code>";
  html += formatBytesHuman(fsInfo.pageSize);
  html += "</code></td></tr>";

  html += "<tr><td>Max open files</td><td><code>";
  html += String(fsInfo.maxOpenFiles);
  html += "</code></td></tr>";

  html += "<tr><td>Max path length</td><td><code>";
  html += String(fsInfo.maxPathLength);
  html += "</code></td></tr>";

  html += "</table>";

  appendLittleFsChannelConfigStatus(html);
  appendLittleFsStoredFiles(html);
}

// ============================================================
// HTTP handlers
// ============================================================

void handlePing() {
  noteHttpRequest();

  Serial.println("HTTP /ping");
  Serial.flush();

  sendNoCacheHeaders();
  server.send(200, "text/plain", "pong\n");
}

void handleState() {
  noteHttpRequest();

  Serial.println("HTTP /state");
  Serial.flush();

  String text;
  text.reserve(1600);

  text += "status=";
  text += statusText;
  text += "\n";

  text += "ip=";
  text += ipToString(eth.localIP());
  text += "\n";

  text += "w5500_version=0x";
  text += String(w5500Version, HEX);
  text += "\n";

  text += "ws2812_gp=16\n";
  text += "scpi_port=5025\n";
  text += "littlefs=";
  text += littleFsReady ? "ready" : "not_ready";
  text += "\n";

  text += "cpu_mhz=";
  text += String(F_CPU / 1000000UL);
  text += "\n";

  text += "heap_total_bytes=";
  text += String(getHeapTotalBytes());
  text += "\n";

  text += "heap_used_bytes=";
  text += String(getHeapUsedBytes());
  text += "\n";

  text += "heap_free_bytes=";
  text += String(getHeapFreeBytes());
  text += "\n";

  text += "heap_used_percent=";
  text += String(getHeapUsedPercent(), 1);
  text += "\n";

  text += "core0_load_percent=";
  text += String(runtimeCore0LoadPct, 1);
  text += "\n";

  text += "loop_rate_hz=";
  text += String(runtimeLoopsPerSecond, 1);
  text += "\n";

  text += "loop_busy_max_us=";
  text += String(runtimeLoopMaxUs);
  text += "\n";

  text += "http_request_count=";
  text += String(httpRequestCount);
  text += "\n";

  text += "scpi_command_count=";
  text += String(scpiCommandCount);
  text += "\n";

  text += "core1_state=unused\n";

  text += "configured_ip=";
  text += ipToString(DEVICE_IP);
  text += "\n";

  text += "configured_subnet=";
  text += ipToString(DEVICE_SUBNET);
  text += "\n";

  text += "configured_gateway=";
  text += ipToString(DEVICE_GATEWAY);
  text += "\n";

  text += "configured_dns=";
  text += ipToString(DEVICE_DNS);
  text += "\n\n";

  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    text += "ch";
    text += String(ch + 1);
    text += "=";
    text += hex16(channelMask[ch]);
    text += " resistance=";
    text += calculateOutputResistanceText(ch, channelMask[ch]);
    text += " count=";
    text += String(applyCounter[ch]);
    text += "\n";
  }

  sendNoCacheHeaders();
  server.send(200, "text/plain", text);
}

void handleRoot() {
  noteHttpRequest();

  Serial.println("HTTP /");
  Serial.flush();

  String html;
  html.reserve(10000);

  appendCommonPageHeader(html, "RP2040 Matrix Control");

  html += "<h1>RP2040 Resistor Matrix</h1>";

  html += "<p>Status: <code>";
  html += statusText;
  html += "</code></p>";

  html += "<p>IP: <code>";
  html += ipToString(eth.localIP());
  html += "</code>, HTTP: <code>80</code>, SCPI: <code>5025</code></p>";

  html += "<p>WS2812 heartbeat: <code>GP16</code></p>";

  html += "<p class='warn'>";
  html += "For first hardware test use only 0000 or 0001. ";
  html += "Do not use FFFF until resistor/MOSFET power path is verified.";
  html += "</p>";

  html += "<h2>Manual channel control</h2>";

  html += "<div class='table-scroll'>";
  html += "<table class='control-table'>";
  html += "<tr>";
  html += "<th>Channel</th>";
  html += "<th>Current mask</th>";
  html += "<th>Active 16-bit indicator<br><span class='small'>bit 15 left, bit 0 right</span></th>";
  html += "<th>Calculated output resistance</th>";
  html += "<th>New mask and Apply</th>";
  html += "</tr>";

  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    uint16_t mask = channelMask[ch];

    html += "<tr>";

    html += "<td><b>CH";
    html += String(ch + 1);
    html += "</b></td>";

    html += "<td><code>";
    html += hex16(mask);
    html += "</code></td>";

    html += "<td>";
    appendBitIndicator(html, ch, mask);
    html += "</td>";

    html += "<td><span class='res'>";
    html += calculateOutputResistanceText(ch, mask);
    html += "</span></td>";

    html += "<td>";
    html += "<form action='/set' method='GET'>";
    html += "<input type='hidden' name='ch' value='";
    html += String(ch + 1);
    html += "'>";
    html += "<input class='mask' name='mask' value='";
    html += hex16(mask);
    html += "' maxlength='6' pattern='^(0x|0X)?[0-9A-Fa-f]{1,4}$'>";
    html += "<button class='apply' type='submit'>Apply CH";
    html += String(ch + 1);
    html += "</button>";
    html += "</form>";
    html += "</td>";

    html += "</tr>";
  }

  html += "</table>";
  html += "</div>";

  html += "<form action='/alloff' method='GET' style='margin-top:16px;'>";
  html += "<button class='off' type='submit'>FORCE ALL CHANNELS OFF</button>";
  html += "</form>";

  html += "<p class='small'>";
  html += "Resistance is calculated as the parallel equivalent of all active resistor branches. ";
  html += "Mask 0x0000 means all MOSFETs OFF and output is OPEN.";
  html += "</p>";

  html += "<h2>Current channel resistor tables</h2>";
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    html += "<h3>CH";
    html += String(ch + 1);
    html += "</h3>";
    html += "<table>";
    html += "<tr><th>Bit</th><th>Branch</th><th>Resistance</th></tr>";

    for (uint8_t i = 0; i < BIT_COUNT; i++) {
      html += "<tr><td>";
      html += String(channelResistorTable[ch][i].bit);
      html += "</td><td>";
      html += channelResistorTable[ch][i].mosfet_name;
      html += "</td><td>";
      html += channelResistorTable[ch][i].nominal_resistance;
      html += "</td></tr>";
    }

    html += "</table>";
  }

  appendCommonPageFooter(html);

  sendNoCacheHeaders();
  server.send(200, "text/html", html);
}


void appendNetworkSettingsForm(String& html) {
  html += "<div class='card'>";
  html += "<h2>Ethernet interface</h2>";
  html += "<p class='small'>Default mode is static IP. Changes are stored to LittleFS and used on next boot. Current active IP remains until power-cycle/restart.</p>";

  html += "<div class='grid'>";
  html += "<div class='metric'><div class='metric-label'>Active IP</div><div class='metric-value'>";
  html += ipToString(eth.localIP());
  html += "</div></div>";
  html += "<div class='metric'><div class='metric-label'>Configured boot IP</div><div class='metric-value'>";
  html += ipToString(DEVICE_IP);
  html += "</div></div>";
  html += "<div class='metric'><div class='metric-label'>Mode</div><div class='metric-value'>Static</div></div>";
  html += "<div class='metric'><div class='metric-label'>SCPI port</div><div class='metric-value'>5025</div></div>";
  html += "</div>";

  html += "<form method='POST' action='/network_save'>";
  html += "<table>";
  html += "<tr><th>Setting</th><th>Value</th></tr>";
  html += "<tr><td>Mode</td><td><select name='mode'><option value='static' selected>Static IP</option></select></td></tr>";

  html += "<tr><td>Device IP</td><td><input name='ip' value='";
  html += ipToString(DEVICE_IP);
  html += "' pattern='[0-9.]{7,15}'></td></tr>";

  html += "<tr><td>Subnet mask</td><td><input name='subnet' value='";
  html += ipToString(DEVICE_SUBNET);
  html += "' pattern='[0-9.]{7,15}'></td></tr>";

  html += "<tr><td>Gateway</td><td><input name='gateway' value='";
  html += ipToString(DEVICE_GATEWAY);
  html += "' pattern='[0-9.]{7,15}'></td></tr>";

  html += "<tr><td>DNS</td><td><input name='dns' value='";
  html += ipToString(DEVICE_DNS);
  html += "' pattern='[0-9.]{7,15}'></td></tr>";
  html += "</table>";
  html += "<p><button class='apply' type='submit'>Save Ethernet Settings</button></p>";
  html += "</form>";

  html += "<div class='notice'>";
  html += "<b>Default safe values:</b> IP 192.168.7.50, subnet 255.255.255.0, gateway 0.0.0.0, DNS 0.0.0.0. ";
  html += "If you set an unreachable IP, reflash or erase LittleFS to return to defaults.";
  html += "</div>";

  html += "<h3>Stored network config file</h3>";
  html += "<pre><code>";
  html += networkConfigToText();
  html += "</code></pre>";
  html += "</div>";
}

void handleNetwork() {
  noteHttpRequest();

  Serial.println("HTTP /network");
  Serial.flush();

  String html;
  html.reserve(7000);
  appendCommonPageHeader(html, "RP2040 Matrix Ethernet");

  html += "<h1>Ethernet Settings</h1>";
  html += "<p>Status: <code>";
  html += statusText;
  html += "</code></p>";

  appendNetworkSettingsForm(html);

  appendCommonPageFooter(html);
  sendNoCacheHeaders();
  server.send(200, "text/html", html);
}

void handleNetworkSave() {
  noteHttpRequest();

  Serial.println("HTTP /network_save");
  Serial.flush();

  IPAddress newIp;
  IPAddress newSubnet;
  IPAddress newGateway;
  IPAddress newDns;

  if (!server.hasArg("ip") || !parseIpAddressText(server.arg("ip"), newIp)) {
    server.send(400, "text/plain", "Bad IP address\n");
    return;
  }

  if (!server.hasArg("subnet") || !parseIpAddressText(server.arg("subnet"), newSubnet)) {
    server.send(400, "text/plain", "Bad subnet mask\n");
    return;
  }

  if (!server.hasArg("gateway") || !parseIpAddressText(server.arg("gateway"), newGateway)) {
    server.send(400, "text/plain", "Bad gateway\n");
    return;
  }

  if (!server.hasArg("dns") || !parseIpAddressText(server.arg("dns"), newDns)) {
    server.send(400, "text/plain", "Bad DNS\n");
    return;
  }

  DEVICE_IP = newIp;
  DEVICE_SUBNET = newSubnet;
  DEVICE_GATEWAY = newGateway;
  DEVICE_DNS = newDns;

  bool saved = saveNetworkConfigToLittleFS();

  if (saved) {
    setStatus("Ethernet settings saved. Restart required.");
    clearLastError();
  } else {
    setStatus("Ethernet settings changed in RAM only. LittleFS save failed.");
    setLastError("Ethernet config save failed");
  }

  server.sendHeader("Location", "/network", true);
  server.send(303, "text/plain", "See Other\n");
}

void handleRuntimePage() {
  noteHttpRequest();

  String html;
  html.reserve(5000);
  appendCommonPageHeader(html, "RP2040 Matrix Runtime");
  html += "<h1>Runtime Monitor</h1>";
  appendRuntimeMonitorInfo(html);
  appendCommonPageFooter(html);
  sendNoCacheHeaders();
  server.send(200, "text/html", html);
}

void handleFilesPage() {
  noteHttpRequest();

  String html;
  html.reserve(9000);
  appendCommonPageHeader(html, "RP2040 Matrix Files");
  html += "<h1>LittleFS Files</h1>";
  appendLittleFsStorageInfo(html);
  appendCommonPageFooter(html);
  sendNoCacheHeaders();
  server.send(200, "text/html", html);
}

void handleSettings() {
  noteHttpRequest();

  Serial.println("HTTP /settings");
  Serial.flush();

  String selectedChStr = server.hasArg("ch") ? server.arg("ch") : "1";
  int selectedCh = selectedChStr.toInt();
  if (selectedCh < 1 || selectedCh > 8) {
    selectedCh = 1;
  }

  uint8_t chIndex = uint8_t(selectedCh - 1);
  String currentConfig = channelConfigToText(chIndex);

  String html;
  html.reserve(13000);

  appendCommonPageHeader(html, "RP2040 Matrix Settings");

  html += "<h1>Settings</h1>";

  html += "<p>Status: <code>";
  html += statusText;
  html += "</code></p>";

  html += "<div class='card'>";

  html += "<h2>Upload per-channel ResistorBitInfo table</h2>";
  html += "<p class='small'>";
  html += "Runtime firmware cannot compile a C++ .h file after upload. ";
  html += "This page accepts either CSV lines or copied C++ initializer lines from a header file, ";
  html += "then stores the parsed values as the channel runtime resistor table.";
  html += "</p>";

  html += "<form method='GET' action='/settings'>";
  html += "View channel: <select name='ch'>";
  for (uint8_t ch = 1; ch <= CHANNEL_COUNT; ch++) {
    html += "<option value='";
    html += String(ch);
    html += "'";
    if (ch == selectedCh) {
      html += " selected";
    }
    html += ">CH";
    html += String(ch);
    html += "</option>";
  }
  html += "</select>";
  html += "<button type='submit'>Load</button>";
  html += "</form>";

  html += "<form method='POST' action='/upload_config'>";
  html += "<p>Upload to channel: <select name='ch'>";
  for (uint8_t ch = 1; ch <= CHANNEL_COUNT; ch++) {
    html += "<option value='";
    html += String(ch);
    html += "'";
    if (ch == selectedCh) {
      html += " selected";
    }
    html += ">CH";
    html += String(ch);
    html += "</option>";
  }
  html += "</select></p>";

  html += "<p>";
  html += "<input type='file' id='fileInput' accept='.csv,.h,.txt'>";
  html += "<span class='small'>Choose a CSV/.h file; browser will load it into the text box before upload.</span>";
  html += "</p>";

  html += "<textarea id='configText' name='config'>";
  html += currentConfig;
  html += "</textarea>";

  html += "<p>";
  html += "<button class='apply' type='submit'>Upload / Save Channel Config</button>";
  html += "</p>";
  html += "</form>";

  html += "<h2>Expected CSV format</h2>";
  html += "<pre><code>";
  html += "bit,mosfet_name,nominal_resistance\n";
  html += "0,Q16,626R\n";
  html += "1,Q15,1.24k\n";
  html += "...\n";
  html += "15,Q1,20M\n";
  html += "</code></pre>";

  html += "<h2>Accepted .h initializer line format</h2>";
  html += "<pre><code>";
  html += "{0,  \"Q16\", \"626R\"},\n";
  html += "{1,  \"Q15\", \"1.24k\"},\n";
  html += "...\n";
  html += "{15, \"Q1\",  \"20M\"},\n";
  html += "</code></pre>";

  html += "<script>";
  html += "document.getElementById('fileInput').addEventListener('change', function(evt){";
  html += "var f=evt.target.files[0]; if(!f) return;";
  html += "var r=new FileReader();";
  html += "r.onload=function(e){document.getElementById('configText').value=e.target.result;};";
  html += "r.readAsText(f);";
  html += "});";
  html += "</script>";

  html += "</div>";

  appendCommonPageFooter(html);

  sendNoCacheHeaders();
  server.send(200, "text/html", html);
}

void handleUploadConfig() {
  noteHttpRequest();

  Serial.println("HTTP /upload_config");
  Serial.flush();

  uint8_t ch = 0;
  if (!parseChannel(ch)) {
    server.send(400, "text/plain", "Bad channel. Use ch=1..8\n");
    return;
  }

  if (!server.hasArg("config")) {
    server.send(400, "text/plain", "Missing config text\n");
    return;
  }

  String configText = server.arg("config");
  char error[96];

  if (!parseChannelConfigText(ch, configText, error, sizeof(error))) {
    setStatus(error);
    setLastError(error);
    server.send(400, "text/plain", String("Config parse failed: ") + error + "\n");
    return;
  }

  bool saved = saveChannelConfigToLittleFS(ch);

  char msg[128];
  snprintf(
    msg,
    sizeof(msg),
    "CH%u config uploaded%s",
    unsigned(ch + 1),
    saved ? " and saved" : " to RAM only"
  );

  setStatus(msg);
  clearLastError();

  Serial.println(msg);
  Serial.flush();

  redirectToSettings();
}

void handleDownloadConfig() {
  noteHttpRequest();

  uint8_t ch = 0;
  if (!parseChannel(ch)) {
    server.send(400, "text/plain", "Bad channel. Use ch=1..8\n");
    return;
  }

  sendNoCacheHeaders();
  server.send(200, "text/plain", channelConfigToText(ch));
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
  clearLastError();
  setLedMode(LED_OK);

  redirectToRoot();
}

void handleAllOff() {
  noteHttpRequest();

  Serial.println("HTTP /alloff");
  Serial.flush();

  forceAllOff();
  clearLastError();

  redirectToRoot();
}

void handleNotFound() {
  noteHttpRequest();

  Serial.print("HTTP 404: ");
  Serial.println(server.uri());
  Serial.flush();

  server.send(404, "text/plain", "404 Not Found\n");
}

void setupHttpServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/ping", HTTP_GET, handlePing);
  server.on("/state", HTTP_GET, handleState);
  server.on("/set", HTTP_GET, handleSet);
  server.on("/alloff", HTTP_GET, handleAllOff);
  server.on("/settings", HTTP_GET, handleSettings);
  server.on("/network", HTTP_GET, handleNetwork);
  server.on("/network_save", HTTP_POST, handleNetworkSave);
  server.on("/runtime", HTTP_GET, handleRuntimePage);
  server.on("/files", HTTP_GET, handleFilesPage);
  server.on("/upload_config", HTTP_POST, handleUploadConfig);
  server.on("/config", HTTP_GET, handleDownloadConfig);
  server.onNotFound(handleNotFound);

  server.begin();

  Serial.println("HTTP server started on port 80");
  Serial.flush();
}

// ============================================================
// SCPI server
// ============================================================

void scpiPrintHelp(WiFiClient& client) {
  client.println("Commands:");
  client.println("*IDN?");
  client.println("*CLS");
  client.println("SYST:ERR?");
  client.println("STATE?");
  client.println("ALL:OFF");
  client.println("CH<n>:MASK?");
  client.println("CH<n>:MASK <hex>");
  client.println("CH<n>:RES?");
  client.println("CH<n>:CONF?");
  client.println("Example: CH1:MASK 0001");
}

bool parseScpiChannelCommand(String cmd, uint8_t& channelIndex, String& rest) {
  cmd.trim();

  while (cmd.startsWith(":")) {
    cmd.remove(0, 1);
    cmd.trim();
  }

  String upper = cmd;
  upper.toUpperCase();

  if (!upper.startsWith("CH")) {
    return false;
  }

  int pos = 2;
  if (pos >= int(upper.length()) || !isdigit(upper[pos])) {
    return false;
  }

  int ch = 0;
  while (pos < int(upper.length()) && isdigit(upper[pos])) {
    ch = ch * 10 + (upper[pos] - '0');
    pos++;
  }

  if (ch < 1 || ch > 8) {
    return false;
  }

  channelIndex = uint8_t(ch - 1);
  rest = cmd.substring(pos);
  rest.trim();

  return true;
}

void processScpiLine(const char* rawLine, WiFiClient& client) {
  String cmd(rawLine);
  cmd.trim();

  if (cmd.length() == 0) {
    return;
  }

  noteScpiCommand();

  Serial.print("SCPI: ");
  Serial.println(cmd);
  Serial.flush();

  String upper = cmd;
  upper.toUpperCase();

  if (upper == "*IDN?") {
    client.println("OpenBench,RP2040-Resistor-Matrix,000001,0.2");
    return;
  }

  if (upper == "*CLS") {
    clearLastError();
    client.println("OK");
    return;
  }

  if (upper == "SYST:ERR?" || upper == "SYSTEM:ERROR?") {
    client.println(lastError);
    clearLastError();
    return;
  }

  if (upper == "HELP?" || upper == "HELP") {
    scpiPrintHelp(client);
    return;
  }

  if (upper == "STATE?") {
    for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
      client.print("CH");
      client.print(ch + 1);
      client.print("=");
      client.print(hex16(channelMask[ch]));
      client.print(",");
      client.print(calculateOutputResistanceText(ch, channelMask[ch]));
      if (ch < CHANNEL_COUNT - 1) {
        client.print(";");
      }
    }
    client.println();
    return;
  }

  if (upper == "ALL:OFF" || upper == ":ALL:OFF" || upper == "OUTP:ALL OFF" || upper == "OUTPUT:ALL OFF") {
    forceAllOff();
    clearLastError();
    client.println("OK");
    return;
  }

  uint8_t ch = 0;
  String rest;

  if (!parseScpiChannelCommand(cmd, ch, rest)) {
    setLastError("-113,\"Undefined header\"");
    client.println("ERR,-113,\"Undefined header\"");
    return;
  }

  String restUpper = rest;
  restUpper.toUpperCase();

  while (restUpper.startsWith(":")) {
    restUpper.remove(0, 1);
    rest.remove(0, 1);
    rest.trim();
  }

  if (restUpper == "MASK?") {
    client.println(hex16(channelMask[ch]));
    return;
  }

  if (restUpper.startsWith("MASK")) {
    String valuePart = rest.substring(4);
    valuePart.trim();

    if (valuePart.startsWith("=")) {
      valuePart.remove(0, 1);
      valuePart.trim();
    }

    uint16_t mask = 0;
    if (!parseHex16String(valuePart, mask)) {
      setLastError("-128,\"Numeric data not allowed\"");
      client.println("ERR,-128,\"Bad mask\"");
      return;
    }

    if (!applyChannelMask(ch, mask)) {
      setLastError("-350,\"Queue overflow\"");
      client.println("ERR,-350,\"Apply failed\"");
      return;
    }

    char msg[96];
    snprintf(
      msg,
      sizeof(msg),
      "CH%u set to 0x%04X by SCPI",
      unsigned(ch + 1),
      unsigned(mask)
    );
    setStatus(msg);
    clearLastError();

    client.println("OK");
    return;
  }

  if (restUpper == "RES?" || restUpper == "RESISTANCE?") {
    client.println(calculateOutputResistanceText(ch, channelMask[ch]));
    return;
  }

  if (restUpper == "CONF?" || restUpper == "CONFIG?") {
    client.print(channelConfigToText(ch));
    return;
  }

  setLastError("-113,\"Undefined header\"");
  client.println("ERR,-113,\"Undefined header\"");
}

void setupScpiServer() {
  scpiServer.begin();
  Serial.println("SCPI server started on TCP port 5025");
  Serial.flush();
}

void handleScpiServer() {
  if (!scpiClient || !scpiClient.connected()) {
    WiFiClient newClient = scpiServer.accept();
    if (newClient) {
      scpiClient = newClient;
      scpiLineLen = 0;
      scpiClient.println("RP2040 Resistor Matrix SCPI ready");
      Serial.println("SCPI client connected");
    }
    return;
  }

  while (scpiClient.available() > 0) {
    char c = char(scpiClient.read());

    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      scpiLine[scpiLineLen] = '\0';
      processScpiLine(scpiLine, scpiClient);
      scpiLineLen = 0;
      continue;
    }

    if (scpiLineLen < sizeof(scpiLine) - 1) {
      scpiLine[scpiLineLen++] = c;
    } else {
      scpiLineLen = 0;
      setLastError("-350,\"Input buffer overflow\"");
      scpiClient.println("ERR,-350,\"Input buffer overflow\"");
    }
  }
}

// ============================================================
// Arduino setup / loop
// ============================================================

void setup() {
  Serial.begin(115200);
  delay(3000);

  heartbeatBegin();
  setLedMode(LED_BOOT);

  Serial.println();
  Serial.println("RP2040-Zero W5500 resistor matrix HTTP + SCPI + WS2812");
  Serial.flush();

  littleFsReady = LittleFS.begin();
  Serial.print("LittleFS: ");
  Serial.println(littleFsReady ? "ready" : "not ready");
  Serial.flush();

  if (loadNetworkConfigFromLittleFS()) {
    Serial.print("Network config loaded from LittleFS, boot IP: ");
    Serial.println(ipToString(DEVICE_IP));
  } else {
    Serial.println("Network config: using default static IP");
  }
  Serial.flush();

  loadAllRuntimeConfigs();
  runtimeMonitorBegin();

  setupShiftRegisters();

  Serial.println("STEP 2: Ethernet CS/INT setup");
  Serial.flush();

  pinMode(ETH_CS, OUTPUT);
  digitalWrite(ETH_CS, HIGH);

  pinMode(ETH_INT, INPUT_PULLUP);

  Serial.println("STEP 3: SPI0 pin setup");
  Serial.flush();

  SPI.setRX(ETH_MISO);
  SPI.setCS(ETH_CS);
  SPI.setSCK(ETH_SCK);
  SPI.setTX(ETH_MOSI);
  SPI.begin();

  Serial.println("SPI0 pins configured:");
  Serial.println("  MISO GP0");
  Serial.println("  CS   GP1");
  Serial.println("  SCK  GP2");
  Serial.println("  MOSI GP3");
  Serial.flush();

  Serial.println("STEP 4: W5500 probe");
  Serial.flush();

  if (!w5500SoftwareResetAndProbe()) {
    Serial.println("Ethernet hardware/link failed. HTTP/SCPI servers not started.");
    Serial.flush();
    return;
  }

  Serial.println("STEP 5: Ethernet static start");
  Serial.flush();

  if (!startEthernetStatic()) {
    Serial.println("Ethernet static setup failed. HTTP/SCPI servers not started.");
    Serial.flush();
    return;
  }

  Serial.println("STEP 6: HTTP and SCPI server start");
  Serial.flush();

  setupHttpServer();
  setupScpiServer();

  Serial.println();
  Serial.print("HTTP: http://");
  Serial.print(ipToString(eth.localIP()));
  Serial.println("/");

  Serial.print("SCPI: ");
  Serial.print(ipToString(eth.localIP()));
  Serial.println(":5025");
  Serial.flush();

  Serial.println("STEP 7: forcing all channels OFF");
  Serial.flush();

  forceAllOff();

  Serial.println("STEP 8: all channels OFF done");
  Serial.flush();

  setLedMode(LED_OK);
}

void loop() {
  uint32_t busyStartUs = micros();

  updateHeartbeat();

  if (!ethernetFault) {
    server.handleClient();
    handleScpiServer();
  }

  uint32_t busyUs = micros() - busyStartUs;
  runtimeMonitorUpdate(busyUs);

  delay(1);
}
