/*
  RP2040-Zero + W5500 direct static HTTP test

  Arduino core:
    Earle Philhower Raspberry Pi Pico/RP2040 core

  Network:
    No router
    No DHCP
    No gateway
    No DNS
    Direct PC <-> W5500 Ethernet connection

  PC manual Ethernet settings:
    IP address: 192.168.7.10
    Subnet:     255.255.255.0
    Gateway:    empty
    DNS:        empty

  W5500 static IP:
    192.168.7.50

  Wiring:
    W5500 MISO -> RP2040 GP0
    W5500 CS   -> RP2040 GP1
    W5500 SCK  -> RP2040 GP2
    W5500 MOSI -> RP2040 GP3
    W5500 INT  -> RP2040 GP4, unused in this version
    W5500 RST  -> not connected

  Requirements:
    - W5500 only
    - SPI0
    - SPI mode 0
    - 8-bit transfers
    - CS active low
    - 1 MHz during W5500 reset/probe
    - 4 MHz runtime debug speed
    - Polling mode
    - No reset GPIO
    - W5500 software reset only
*/

#include <Arduino.h>
#include <SPI.h>
#include <W5500lwIP.h>
#include <WiFiClient.h>
#include <WebServer.h>

// ---------------- Pin map ----------------
static constexpr uint8_t ETH_MISO = 0;
static constexpr uint8_t ETH_CS   = 1;
static constexpr uint8_t ETH_SCK  = 2;
static constexpr uint8_t ETH_MOSI = 3;
static constexpr uint8_t ETH_INT  = 4;   // not used, polling only

// ---------------- SPI speed ----------------
static constexpr uint32_t ETH_INIT_SPI_HZ    = 1000000UL;  // 1 MHz
static constexpr uint32_t ETH_RUNTIME_SPI_HZ = 4000000UL;  // 4 MHz for debug

// ---------------- Static direct-link IP config ----------------
// RP2040-W5500 address
static IPAddress DEVICE_IP(192, 168, 7, 50);

// No router/gateway/DNS in direct PC connection
static IPAddress DEVICE_DNS(0, 0, 0, 0);
static IPAddress DEVICE_GATEWAY(0, 0, 0, 0);
static IPAddress DEVICE_SUBNET(255, 255, 255, 0);

// ---------------- Ethernet + HTTP ----------------
Wiznet5500lwIP eth(ETH_CS);
WebServer httpServer(80);

static bool ethernetFault = false;
static uint8_t w5500Version = 0;

// ---------------- Safe state placeholder ----------------
void enterSafeOutputState(const char* reason) {
  Serial.print("SAFE OUTPUT STATE: ");
  Serial.println(reason);

  // Later for real firmware:
  // - switch all output MOSFETs/relays OFF
  // - force shift-register output mask to 0
  // - block output enable until system is healthy
}

// ---------------- Utility ----------------
String ipToString(const IPAddress& ip) {
  return String(ip[0]) + "." +
         String(ip[1]) + "." +
         String(ip[2]) + "." +
         String(ip[3]);
}

// ---------------- Low-level W5500 common register access ----------------
//
// W5500 SPI frame:
//   address high byte
//   address low byte
//   control byte
//   data
//
// Common register block:
//   read  control byte = 0x00
//   write control byte = 0x04

void w5500WriteCommonReg(uint16_t address, uint8_t value, uint32_t spiHz) {
  SPI.beginTransaction(SPISettings(spiHz, MSBFIRST, SPI_MODE0));

  digitalWrite(ETH_CS, LOW);

  SPI.transfer((address >> 8) & 0xFF);
  SPI.transfer(address & 0xFF);
  SPI.transfer(0x04);  // common register block, write, variable data mode
  SPI.transfer(value);

  digitalWrite(ETH_CS, HIGH);

  SPI.endTransaction();
}

uint8_t w5500ReadCommonReg(uint16_t address, uint32_t spiHz) {
  SPI.beginTransaction(SPISettings(spiHz, MSBFIRST, SPI_MODE0));

  digitalWrite(ETH_CS, LOW);

  SPI.transfer((address >> 8) & 0xFF);
  SPI.transfer(address & 0xFF);
  SPI.transfer(0x00);  // common register block, read, variable data mode
  uint8_t value = SPI.transfer(0x00);

  digitalWrite(ETH_CS, HIGH);

  SPI.endTransaction();

  return value;
}

// ---------------- W5500 software reset and hardware check ----------------
bool w5500SoftwareResetAndProbe() {
  Serial.println("W5500: software reset...");

  // W5500 MR register = 0x0000
  // RST bit = bit 7
  w5500WriteCommonReg(0x0000, 0x80, ETH_INIT_SPI_HZ);
  delay(20);

  // W5500 VERSIONR register = 0x0039
  // Expected value = 0x04
  w5500Version = w5500ReadCommonReg(0x0039, ETH_INIT_SPI_HZ);

  Serial.print("W5500 VERSIONR = 0x");
  Serial.println(w5500Version, HEX);

  if (w5500Version != 0x04) {
    ethernetFault = true;
    enterSafeOutputState("W5500 not detected");
    return false;
  }

  // W5500 PHYCFGR register = 0x002E
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

  if (!linkUp) {
    ethernetFault = true;
    enterSafeOutputState("Ethernet link DOWN");
    return false;
  }

  return true;
}

// ---------------- Static Ethernet startup ----------------
bool startEthernetStaticDirect() {
  Serial.println();
  Serial.println("Ethernet: starting STATIC DIRECT mode...");
  Serial.println("No DHCP. No router. No gateway. No DNS.");

  // Polling mode. ETH_INT is not used.
  lwipPollingPeriod(20);

  // Runtime SPI speed.
  // Use 4 MHz first. Increase to 10 MHz after everything works.
  eth.setSPISpeed(ETH_RUNTIME_SPI_HZ);

  // Static config before eth.begin().
  eth.config(
    DEVICE_IP,
    DEVICE_DNS,
    DEVICE_GATEWAY,
    DEVICE_SUBNET
  );

  eth.begin();

  uint32_t start = millis();

  while (millis() - start < 10000) {
    IPAddress ip = eth.localIP();

    Serial.print("connected=");
    Serial.print(eth.connected());
    Serial.print(" ip=");
    Serial.println(ipToString(ip));

    if (ip == DEVICE_IP) {
      Serial.println("Static direct IP configured successfully.");
      return true;
    }

    delay(500);
  }

  ethernetFault = true;
  enterSafeOutputState("Static direct IP failed");
  return false;
}

// ---------------- HTTP handlers ----------------
void handleRoot() {
  String html;
  html.reserve(1600);

  html += "<!doctype html>";
  html += "<html>";
  html += "<head>";
  html += "<meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>RP2040-Zero W5500 Test</title>";
  html += "<style>";
  html += "body{font-family:Arial,sans-serif;margin:2rem;background:#111;color:#eee;}";
  html += ".card{max-width:760px;padding:1.2rem;border:1px solid #444;border-radius:12px;background:#1b1b1b;}";
  html += "code{color:#9ee493;}";
  html += ".ok{color:#72e072;}";
  html += "</style>";
  html += "</head>";
  html += "<body>";
  html += "<div class='card'>";

  html += "<h1>RP2040-Zero + W5500 HTTP Test</h1>";
  html += "<p>Status: <b class='ok'>OK</b></p>";

  html += "<p>Mode: <code>STATIC DIRECT</code></p>";

  html += "<p>Device IP: <code>";
  html += ipToString(eth.localIP());
  html += "</code></p>";

  html += "<p>PC should be: <code>192.168.7.10 / 255.255.255.0</code></p>";

  html += "<p>Gateway: <code>none</code></p>";
  html += "<p>DNS: <code>none</code></p>";

  html += "<p>W5500 VERSIONR: <code>0x";
  html += String(w5500Version, HEX);
  html += "</code></p>";

  html += "<p>SPI pins: <code>MISO GP0, CS GP1, SCK GP2, MOSI GP3</code></p>";

  html += "<p>Runtime SPI clock: <code>";
  html += String(ETH_RUNTIME_SPI_HZ);
  html += " Hz</code></p>";

  html += "<p>Uptime: <code>";
  html += String(millis() / 1000);
  html += " s</code></p>";

  html += "<p><a href='/health'>Open health endpoint</a></p>";

  html += "</div>";
  html += "</body>";
  html += "</html>";

  httpServer.send(200, "text/html", html);
}

void handleHealth() {
  String text;
  text.reserve(400);

  text += "status=OK\n";
  text += "mode=STATIC_DIRECT\n";
  text += "ip=";
  text += ipToString(eth.localIP());
  text += "\n";

  text += "expected_pc_ip=192.168.7.10\n";
  text += "subnet=255.255.255.0\n";
  text += "gateway=none\n";
  text += "dns=none\n";

  text += "w5500_version=0x";
  text += String(w5500Version, HEX);
  text += "\n";

  text += "runtime_spi_hz=";
  text += String(ETH_RUNTIME_SPI_HZ);
  text += "\n";

  text += "uptime_s=";
  text += String(millis() / 1000);
  text += "\n";

  httpServer.send(200, "text/plain", text);
}

void handleNotFound() {
  httpServer.send(404, "text/plain", "404 Not Found\n");
}

void setupHttpServer() {
  httpServer.on("/", HTTP_GET, handleRoot);
  httpServer.on("/health", HTTP_GET, handleHealth);
  httpServer.onNotFound(handleNotFound);

  httpServer.begin();

  Serial.println("HTTP server started on port 80");
}

// ---------------- Arduino setup ----------------
void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("RP2040-Zero W5500 static direct HTTP test");

  enterSafeOutputState("boot");

  pinMode(ETH_CS, OUTPUT);
  digitalWrite(ETH_CS, HIGH);

  pinMode(ETH_INT, INPUT_PULLUP);

  // RP2040 SPI0 remap.
  // Must be called before SPI.begin().
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

  if (!w5500SoftwareResetAndProbe()) {
    Serial.println("Ethernet hardware/link failed. HTTP server not started.");
    return;
  }

  if (!startEthernetStaticDirect()) {
    Serial.println("Ethernet static direct setup failed. HTTP server not started.");
    return;
  }

  setupHttpServer();

  Serial.println();
  Serial.println("Ready.");
  Serial.print("Open browser: http://");
  Serial.print(ipToString(eth.localIP()));
  Serial.println("/");

  Serial.print("Health: http://");
  Serial.print(ipToString(eth.localIP()));
  Serial.println("/health");
}

// ---------------- Arduino loop ----------------
void loop() {
  if (!ethernetFault) {
    httpServer.handleClient();
  }

  delay(1);
}