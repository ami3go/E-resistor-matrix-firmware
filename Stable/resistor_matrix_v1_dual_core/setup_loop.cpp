/**
 * @file setup_loop.cpp
 * @brief Arduino Core 0 setup/loop entry points; Core 1 lives in core1_runtime.cpp.
 */

#include "app.h"

// ============================================================
// Arduino setup / loop
//
// Core 0: Ethernet, HTTP, SCPI, LittleFS, UI, parsing.
// Core 1: deterministic resistor-output hardware engine.
// ============================================================

/**
 * @brief Arduino Core 0 startup entry point for communications and non-real-time services.
 */
void setup() {
  // Initialize every shared cross-core primitive before any startup delay.
  // Arduino-Pico enables USB and USB CDC before it launches Core 1 and before
  // this setup() function is entered, so delaying the transport here provides
  // no USB-enumeration benefit and can let Core 1 exhaust its startup wait.
  initCoreCommandEngine();

  Serial.begin(115200);
  delay(100);

  heartbeatBegin();
  setLedMode(LED_BOOT);

  deviceSerialNumber = makeBoardSerialNumber();

  Serial.println();
  Serial.println("RP2040-Zero W5500 resistor matrix HTTP + SCPI + WS2812");
  Serial.print("Firmware version: ");
  Serial.println(firmwareVersionString());
  Serial.println("Dual-core build: Core0=network/UI, Core1=shift-register engine");
  Serial.print("Board serial number: ");
  Serial.println(deviceSerialNumber);
  Serial.flush();

  bootMillis = millis();
  appendLogEvent("Boot");
  {
    char fwMsg[128];
    snprintf(fwMsg, sizeof(fwMsg), "Firmware %s build %s %s", FIRMWARE_VERSION, FIRMWARE_BUILD_DATE, FIRMWARE_BUILD_TIME);
    appendLogEvent(fwMsg);
  }
  {
    char msg[96];
    snprintf(msg, sizeof(msg), "Serial number %s", deviceSerialNumber.c_str());
    appendLogEvent(msg);
  }
  runtimeMonitorBegin();

  Serial.println("STEP 1: wait for Core 1 hardware engine safe state");
  Serial.flush();

  if (!waitForCore1Startup(CORE_STARTUP_TIMEOUT_MS)) {
    Serial.println("Core 1 hardware engine did not enter safe state. Network services not started.");
    Serial.print("Core 1 startup stage: ");
    Serial.println(coreTransportGetCore1StartupStage());
    Serial.print("Core 1 ready token: 0x");
    Serial.println(coreTransportGetCore1ReadyToken(), HEX);
    Serial.print("Core 1 mirror: shift_registers_ready=");
    Serial.print(shiftRegistersReady ? 1 : 0);
    Serial.print(" outputs_safe=");
    Serial.println(outputsKnownSafe ? 1 : 0);
    Serial.flush();
    safeState("Core 1 hardware engine not ready");
    return;
  }

  Serial.print("Core 1 hardware engine ready; all outputs OFF; startup stage=");
  Serial.println(coreTransportGetCore1StartupStage());
  Serial.flush();
  appendLogEvent("Boot: Core 1 ready, all outputs OFF");

  littleFsReady = LittleFS.begin();
  Serial.print("LittleFS: ");
  Serial.println(littleFsReady ? "ready" : "not ready");
  Serial.flush();
  appendLogEvent(littleFsReady ? "Boot: LittleFS ready" : "Boot: LittleFS not ready");

  if (loadNetworkConfigFromLittleFS()) {
    Serial.print("Network config loaded from LittleFS, boot IP: ");
    Serial.println(ipToString(DEVICE_IP));
    appendLogEvent("Boot: network config loaded from LittleFS");
  } else {
    Serial.println("Network config: using default static IP");
    appendLogEvent("Boot: default static network config");
  }
  Serial.flush();

  loadAllRuntimeConfigs();

  if (loadSafetyConfigFromLittleFS()) {
    Serial.println("Safety config loaded from LittleFS");
    appendLogEvent("Boot: safety config loaded from LittleFS");
  } else {
    Serial.println("Safety config: using defaults");
    appendLogEvent("Boot: default safety config");
  }

  char policyReason[128] = {0};
  if (!installCore1PolicySnapshot(policyReason, sizeof(policyReason))) {
    safeState(policyReason[0] ? policyReason : "Core 1 policy installation failed");
    return;
  }
  appendLogEvent("Boot: immutable Core 1 calibration/safety policy installed");

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
    safeState("Ethernet hardware/link failed");
    return;
  }

  Serial.println("STEP 5: Ethernet static start");
  Serial.flush();

  if (!startEthernetStatic()) {
    Serial.println("Ethernet static setup failed. HTTP/SCPI servers not started.");
    Serial.flush();
    safeState("Ethernet static setup failed");
    return;
  }

  Serial.println("STEP 6: HTTP and SCPI server start");
  Serial.flush();

  setupHttpServer();
  setupScpiServer();
  appendLogEvent("Boot: HTTP and SCPI servers started");

  Serial.println();
  Serial.print("HTTP: http://");
  Serial.print(ipToString(eth.localIP()));
  Serial.println("/");

  Serial.print("SCPI: ");
  Serial.print(ipToString(eth.localIP()));
  Serial.println(":5025");
  Serial.flush();

  Serial.println("STEP 7: request all channels OFF via Core 1");
  Serial.flush();

  char allOffReason[128] = {0};
  if (!forceAllOff(allOffReason, sizeof(allOffReason))) {
    safeState(allOffReason[0] ? allOffReason : "Startup all-OFF failed");
    return;
  }

  Serial.println("STEP 8: all channels OFF done");
  Serial.flush();
  appendLogEvent("Boot: startup all-OFF command completed");

  setLedMode(LED_OK);
  appendLogEvent("Boot: complete, LED status OK");
}

/**
 * @brief Arduino Core 0 loop for HTTP, SCPI, heartbeat, and monitoring tasks.
 */
void loop() {
  uint32_t busyStartUs = micros();

  coreTransportKickCore0Heartbeat();
  refreshCore0OutputMirror();
  updateHeartbeat();
  drainCore1Events();

  if (!ethernetFault) {
    server.handleClient();
    handleScpiServer();
  }

  uint32_t busyUs = micros() - busyStartUs;
  runtimeMonitorUpdate(busyUs);

  delay(1);
}

