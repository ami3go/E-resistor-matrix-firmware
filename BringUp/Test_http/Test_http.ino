/*
  RP2040-Zero + W5500 + 8-channel 74HC595 resistor matrix
  Static direct HTTP control with WS2812 heartbeat

  Arduino core:
    Earle Philhower Raspberry Pi Pico/RP2040 core

  Required library:
    Adafruit NeoPixel

  PC Ethernet manual config:
    IP:      192.168.7.10
    Subnet:  255.255.255.0
    Gateway: empty
    DNS:     empty

  Device:
    http://192.168.7.50/

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
    SRCLR GP15 active low

  Latch pins:
    CH1 GP5
    CH2 GP6
    CH3 GP7
    CH4 GP8
    CH5 GP9
    CH6 GP10
    CH7 GP13
    CH8 GP14

  WS2812:
    GP16

  Mask behavior:
    bit 0  -> Q16, lowest branch, 626R
    bit 15 -> Q1, highest branch, 20M

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
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <ctype.h>

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
static constexpr uint16_t SR_RESET_PULSE_US       = 20;
static constexpr uint16_t BREAK_BEFORE_MAKE_MS    = 2;

// ============================================================
// Static direct IP
// ============================================================

static IPAddress DEVICE_IP(192, 168, 7, 50);
static IPAddress DEVICE_DNS(0, 0, 0, 0);
static IPAddress DEVICE_GATEWAY(0, 0, 0, 0);
static IPAddress DEVICE_SUBNET(255, 255, 255, 0);

// ============================================================
// Ethernet / HTTP
// ============================================================

Wiznet5500lwIP eth(ETH_CS);
WebServer server(80);

// ============================================================
// State
// ============================================================

static bool ethernetFault = false;
static uint8_t w5500Version = 0;

static uint16_t channelMask[CHANNEL_COUNT] = {
  0, 0, 0, 0, 0, 0, 0, 0
};

static uint32_t applyCounter[CHANNEL_COUNT] = {
  0, 0, 0, 0, 0, 0, 0, 0
};

static char statusText[96] = "Boot";

// ============================================================
// Utility
// ============================================================

String ipToString(const IPAddress& ip) {
  return String(ip[0]) + "." +
         String(ip[1]) + "." +
         String(ip[2]) + "." +
         String(ip[3]);
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

void safeState(const char* reason) {
  Serial.print("SAFE STATE: ");
  Serial.println(reason);
  Serial.flush();

  setStatus(reason);
  ledMode = LED_FAULT;
}

// ============================================================
// WS2812 heartbeat helpers
// ============================================================

void ws2812Set(uint8_t r, uint8_t g, uint8_t b) {
  heartbeatPixel.setPixelColor(0, heartbeatPixel.Color(r, g, b));
  heartbeatPixel.show();
}

void heartbeatBegin() {
  heartbeatPixel.begin();
  heartbeatPixel.setBrightness(24);
  ws2812Set(0, 0, 8);  // dim blue at boot
}

void setLedMode(LedMode mode) {
  ledMode = mode;
  lastLedUpdateMs = 0;
}

void updateHeartbeat() {
  uint32_t now = millis();

  uint16_t intervalMs = 500;

  if (ledMode == LED_ACTIVITY) {
    intervalMs = 120;
  } else if (ledMode == LED_FAULT) {
    intervalMs = 250;
  } else if (ledMode == LED_BOOT) {
    intervalMs = 400;
  } else {
    intervalMs = 700;
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
      ws2812Set(0, 0, 16);    // blue blink
      break;

    case LED_OK:
      ws2812Set(0, 18, 0);    // green heartbeat
      break;

    case LED_ACTIVITY:
      ws2812Set(18, 12, 0);   // yellow fast blink
      break;

    case LED_FAULT:
    default:
      ws2812Set(24, 0, 0);    // red blink
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
  SPI.transfer(0x04);  // common register block, write, VDM
  SPI.transfer(value);

  digitalWrite(ETH_CS, HIGH);

  SPI.endTransaction();
}

uint8_t w5500ReadCommonReg(uint16_t address, uint32_t spiHz) {
  SPI.beginTransaction(SPISettings(spiHz, MSBFIRST, SPI_MODE0));

  digitalWrite(ETH_CS, LOW);

  SPI.transfer((address >> 8) & 0xFF);
  SPI.transfer(address & 0xFF);
  SPI.transfer(0x00);  // common register block, read, VDM
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
// Shift-register low-level control
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
  for (uint8_t bit = 0; bit < 16; bit++) {
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

  // Step 1: selected channel OFF
  Serial.println("applyChannelMask: break-before-make OFF");
  Serial.flush();

  if (!latchMaskToChannel(channelIndex, 0x0000)) {
    Serial.println("applyChannelMask: OFF latch failed");
    Serial.flush();
    return false;
  }

  channelMask[channelIndex] = 0x0000;

  // Step 2: wait
  Serial.println("applyChannelMask: wait BBM");
  Serial.flush();

  delay(BREAK_BEFORE_MAKE_MS);

  // Step 3/4: new mask
  Serial.println("applyChannelMask: latch new mask");
  Serial.flush();

  if (!latchMaskToChannel(channelIndex, newMask)) {
    Serial.println("applyChannelMask: new latch failed");
    Serial.flush();
    return false;
  }

  // Step 5: update shadow after successful latch
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
  digitalWrite(SR_RESET, HIGH);   // inactive
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
  // Later, after hardware is confirmed, SRCLR pulse can be re-enabled.
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
// HTTP parsing
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

bool parseMask(uint16_t& mask) {
  if (!server.hasArg("mask")) {
    return false;
  }

  String s = server.arg("mask");
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

// ============================================================
// HTTP handlers
// ============================================================

void handlePing() {
  Serial.println("HTTP /ping");
  Serial.flush();

  server.send(200, "text/plain", "pong\n");
}

void handleState() {
  Serial.println("HTTP /state");
  Serial.flush();

  String text;
  text.reserve(900);

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

  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    text += "ch";
    text += String(ch + 1);
    text += "=";
    text += hex16(channelMask[ch]);
    text += " count=";
    text += String(applyCounter[ch]);
    text += "\n";
  }

  server.send(200, "text/plain", text);
}

void handleRoot() {
  Serial.println("HTTP /");
  Serial.flush();

  String html;
  html.reserve(3200);

  html += "<!doctype html><html><head>";
  html += "<meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>RP2040 Matrix</title>";
  html += "<style>";
  html += "body{font-family:Arial;margin:24px;background:#111;color:#eee;}";
  html += "input,select,button{font-size:18px;padding:8px;margin:4px;}";
  html += "table{border-collapse:collapse;margin-top:16px;}";
  html += "td,th{border:1px solid #555;padding:6px 10px;}";
  html += "code{color:#9ee493;}";
  html += ".warn{color:#ffd37a;}";
  html += "</style>";
  html += "</head><body>";

  html += "<h1>RP2040 Resistor Matrix</h1>";

  html += "<p>Status: <code>";
  html += statusText;
  html += "</code></p>";

  html += "<p>IP: <code>";
  html += ipToString(eth.localIP());
  html += "</code></p>";

  html += "<p>WS2812 heartbeat: <code>GP16</code></p>";

  html += "<p class='warn'>For first test use only mask 0000 or 0001. Do not test FFFF until hardware power path is verified.</p>";

  html += "<form action='/set' method='GET'>";
  html += "<label>Channel:</label>";
  html += "<select name='ch'>";
  for (uint8_t ch = 1; ch <= CHANNEL_COUNT; ch++) {
    html += "<option value='";
    html += String(ch);
    html += "'>CH";
    html += String(ch);
    html += "</option>";
  }
  html += "</select>";

  html += "<label>Mask hex:</label>";
  html += "<input name='mask' value='0000' maxlength='6'>";
  html += "<button type='submit'>Apply</button>";
  html += "</form>";

  html += "<form action='/alloff' method='GET'>";
  html += "<button type='submit'>ALL OFF</button>";
  html += "</form>";

  html += "<h2>Current masks</h2>";
  html += "<table>";
  html += "<tr><th>CH</th><th>Mask</th><th>Count</th></tr>";

  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    html += "<tr><td>";
    html += String(ch + 1);
    html += "</td><td><code>";
    html += hex16(channelMask[ch]);
    html += "</code></td><td>";
    html += String(applyCounter[ch]);
    html += "</td></tr>";
  }

  html += "</table>";

  html += "<h2>Bit mapping</h2>";
  html += "<table>";
  html += "<tr><th>Bit</th><th>Branch</th><th>Resistance</th></tr>";
  for (uint8_t i = 0; i < 16; i++) {
    html += "<tr><td>";
    html += String(RESISTOR_BIT_TABLE[i].bit);
    html += "</td><td>";
    html += RESISTOR_BIT_TABLE[i].branch_name;
    html += "</td><td>";
    html += RESISTOR_BIT_TABLE[i].nominal_resistance;
    html += "</td></tr>";
  }
  html += "</table>";

  html += "<p>";
  html += "<a href='/ping'>ping</a> | ";
  html += "<a href='/state'>state</a>";
  html += "</p>";

  html += "</body></html>";

  server.send(200, "text/html", html);
}

void handleSet() {
  Serial.println("HTTP /set begin");
  Serial.flush();

  setLedMode(LED_ACTIVITY);

  Serial.print("URI: ");
  Serial.println(server.uri());
  Serial.flush();

  Serial.print("ARGS: ");
  Serial.println(server.args());
  Serial.flush();

  for (uint8_t i = 0; i < server.args(); i++) {
    Serial.print("  arg ");
    Serial.print(i);
    Serial.print(": ");
    Serial.print(server.argName(i));
    Serial.print(" = ");
    Serial.println(server.arg(i));
    Serial.flush();
  }

  uint8_t ch = 0;
  uint16_t mask = 0;

  Serial.println("parse channel...");
  Serial.flush();

  if (!parseChannel(ch)) {
    Serial.println("Bad channel");
    Serial.flush();

    setLedMode(LED_FAULT);
    server.send(400, "text/plain", "Bad channel. Use ch=1..8\n");
    return;
  }

  Serial.println("parse mask...");
  Serial.flush();

  if (!parseMask(mask)) {
    Serial.println("Bad mask");
    Serial.flush();

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
    Serial.println("Apply failed");
    Serial.flush();

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

  Serial.println(msg);
  Serial.println("HTTP /set send response");
  Serial.flush();

  String response;
  response.reserve(160);
  response += "OK\n";
  response += msg;
  response += "\n";
  response += "Back: http://";
  response += ipToString(eth.localIP());
  response += "/\n";

  server.send(200, "text/plain", response);

  Serial.println("HTTP /set done");
  Serial.flush();

  setLedMode(LED_OK);
}

void handleAllOff() {
  Serial.println("HTTP /alloff");
  Serial.flush();

  forceAllOff();

  server.send(200, "text/plain", "OK\nAll channels OFF\n");

  Serial.println("HTTP /alloff done");
  Serial.flush();
}

void handleNotFound() {
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
  server.onNotFound(handleNotFound);

  server.begin();

  Serial.println("HTTP server started on port 80");
  Serial.flush();
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
  Serial.println("RP2040-Zero W5500 resistor matrix HTTP + WS2812 heartbeat");
  Serial.flush();

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
    Serial.println("Ethernet hardware/link failed. HTTP server not started.");
    Serial.flush();
    return;
  }

  Serial.println("STEP 5: Ethernet static start");
  Serial.flush();

  if (!startEthernetStatic()) {
    Serial.println("Ethernet static setup failed. HTTP server not started.");
    Serial.flush();
    return;
  }

  Serial.println("STEP 6: HTTP server start");
  Serial.flush();

  setupHttpServer();

  Serial.println();
  Serial.println("Ready BEFORE forceAllOff.");
  Serial.print("Open: http://");
  Serial.print(ipToString(eth.localIP()));
  Serial.println("/");
  Serial.flush();

  Serial.println("STEP 7: forcing all channels OFF");
  Serial.flush();

  forceAllOff();

  Serial.println("STEP 8: all channels OFF done");
  Serial.flush();

  setLedMode(LED_OK);
}

void loop() {
  updateHeartbeat();

  if (!ethernetFault) {
    server.handleClient();
  }

  delay(1);
}