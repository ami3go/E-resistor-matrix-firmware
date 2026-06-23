/**
 * @file w5500_registers.cpp
 * @brief Low-level W5500 common-register helper functions used during Ethernet bring-up diagnostics.
 */

#include "app.h"

// 05_w5500_registers.ino
// Split from rp2040_w5500_resistor_matrix_v1_safety_logic.ino.
// Keep all files in the same Arduino sketch folder.

// ============================================================
// W5500 low-level register access
// ============================================================

/**
 * @brief Write one W5500 common-register byte through SPI.
 * @param address Function parameter.
 * @param value Function parameter.
 * @param spiHz Function parameter.
 */
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

/**
 * @brief Read one W5500 common-register byte through SPI.
 * @param address Function parameter.
 * @param spiHz Function parameter.
 * @return Result value; for bool, true means the operation succeeded.
 */
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

/**
 * @brief Issue W5500 software reset and verify VERSIONR communication.
 * @return Result value; for bool, true means the operation succeeded.
 */
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

