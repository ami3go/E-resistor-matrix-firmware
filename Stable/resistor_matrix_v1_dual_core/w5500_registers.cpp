/**
 * @file w5500_registers.cpp
 * @brief Low-level W5500 common-register helper functions used during Ethernet bring-up diagnostics.
 */

#include "app.h"


// ============================================================
// W5500 low-level register access
// ============================================================

/**
 * @brief Write one W5500 common-register byte through SPI.
 * @param address W5500 common-register address.
 * @param value Value to write.
 * @param spiHz SPI clock used for the transaction.
 */
void w5500WriteCommonReg(uint16_t address, uint8_t value, uint32_t spiHz) {
  if (ethernetInterfaceStarted) ethernet_arch_lwip_begin();
  SPI.beginTransaction(SPISettings(spiHz, MSBFIRST, SPI_MODE0));

  digitalWrite(ETH_CS, LOW);

  SPI.transfer((address >> 8) & 0xFF);
  SPI.transfer(address & 0xFF);
  SPI.transfer(0x04);
  SPI.transfer(value);

  digitalWrite(ETH_CS, HIGH);

  SPI.endTransaction();
  if (ethernetInterfaceStarted) ethernet_arch_lwip_end();
}

/**
 * @brief Read one W5500 common-register byte through SPI.
 * @param address W5500 common-register address.
 * @param spiHz SPI clock used for the transaction.
 * @return Register value.
 */
uint8_t w5500ReadCommonReg(uint16_t address, uint32_t spiHz) {
  if (ethernetInterfaceStarted) ethernet_arch_lwip_begin();
  SPI.beginTransaction(SPISettings(spiHz, MSBFIRST, SPI_MODE0));

  digitalWrite(ETH_CS, LOW);

  SPI.transfer((address >> 8) & 0xFF);
  SPI.transfer(address & 0xFF);
  SPI.transfer(0x00);
  uint8_t value = SPI.transfer(0x00);

  digitalWrite(ETH_CS, HIGH);

  SPI.endTransaction();
  if (ethernetInterfaceStarted) ethernet_arch_lwip_end();

  return value;
}

/**
 * @brief Read the physical W5500 link state without resetting the interface.
 *
 * VERSIONR is checked first so an all-zero/all-one SPI response is not
 * mistaken for a valid PHY link transition.
 *
 * @param linkUp Receives true when PHYCFGR reports an active cable link.
 * @param phyConfig Optional receiver for the raw PHYCFGR value.
 * @return true when the W5500 responded with the expected VERSIONR value.
 */
bool w5500ReadLinkState(bool& linkUp, uint8_t* phyConfig) {
  const uint8_t version = w5500ReadCommonReg(0x0039, ETH_INIT_SPI_HZ);
  if (version != 0x04U) {
    linkUp = false;
    if (phyConfig) *phyConfig = 0U;
    return false;
  }

  const uint8_t phy = w5500ReadCommonReg(0x002E, ETH_INIT_SPI_HZ);
  linkUp = (phy & 0x01U) != 0U;
  if (phyConfig) *phyConfig = phy;
  return true;
}

/**
 * @brief Issue W5500 software reset and verify hardware communication.
 *
 * An unplugged Ethernet cable is not a hardware failure. The function records
 * and reports PHY state but succeeds whenever VERSIONR confirms that the W5500
 * itself is present. Link-up is handled later by the recovery state machine.
 *
 * @return true when the W5500 hardware responded correctly.
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

  if (w5500Version != 0x04U) {
    ethernetLinkUp = false;
    return false;
  }

  uint8_t phy = 0U;
  bool linkUp = false;
  if (!w5500ReadLinkState(linkUp, &phy)) {
    ethernetLinkUp = false;
    return false;
  }

  ethernetLinkUp = linkUp;

  Serial.print("W5500 PHYCFGR = 0x");
  Serial.println(phy, HEX);

  Serial.print("Ethernet link: ");
  Serial.println(linkUp ? "UP" : "DOWN (recoverable; waiting for cable)");

  Serial.print("Speed: ");
  Serial.println((phy & 0x02U) ? "100M" : "10M");

  Serial.print("Duplex: ");
  Serial.println((phy & 0x04U) ? "FULL" : "HALF");
  Serial.flush();

  return true;
}
