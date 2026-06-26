/**
 * @file ethernet_startup.cpp
 * @brief Static Ethernet startup and W5500/lwIP initialization for the Arduino RP2040 runtime.
 */

#include "app.h"

// 07_ethernet_startup.ino
// Split from rp2040_w5500_resistor_matrix_v1_safety_logic.ino.
// Keep all files in the same Arduino sketch folder.

// ============================================================
// Ethernet startup
// ============================================================

/**
 * @brief Start W5500 Ethernet using the configured static IPv4 settings.
 * @return Result value; for bool, true means the operation succeeded.
 */
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

