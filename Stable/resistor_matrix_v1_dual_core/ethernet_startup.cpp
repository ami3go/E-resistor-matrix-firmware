/**
 * @file ethernet_startup.cpp
 * @brief Static Ethernet startup and recoverable W5500 link management.
 */

#include "app.h"


// ============================================================
// Ethernet startup and auto-recovery
// ============================================================

static void setNetworkLedMode(LedMode mode) {
  if (ledMode == LED_IDENTIFY_BLUE) {
    ledModeBeforeIdentify = mode;
  } else {
    setLedMode(mode);
  }
}

static void closeScpiClientForLinkLoss() {
  if (scpiClient) {
    scpiClient.stop();
  }
  scpiLineLen = 0;
  scpiDiscardUntilNewline = false;
}

static void setRecoverableNetworkError(const char* status, const char* error) {
  ethernetFault = true;
  ethernetNetworkErrorActive = true;
  setStatus(status);
  setLastError(error);
}

static void clearRecoverableNetworkError() {
  ethernetFault = false;
  if (ethernetNetworkErrorActive) {
    if (strstr(lastError, "Ethernet") != nullptr || strstr(lastError, "W5500") != nullptr) {
      clearLastError();
    }
    ethernetNetworkErrorActive = false;
  }
}

const char* ethernetRecoveryStateText(EthernetRecoveryState state) {
  switch (state) {
    case ETHERNET_RECOVERY_HARDWARE_RETRY: return "hardware_retry";
    case ETHERNET_RECOVERY_WAIT_LINK: return "wait_link";
    case ETHERNET_RECOVERY_ONLINE: return "online";
    case ETHERNET_RECOVERY_UNINITIALIZED:
    default: return "uninitialized";
  }
}

static void updateEthernetLinkState(bool linkUp, bool initialSample) {
  const bool previousLinkUp = ethernetLinkUp;
  const EthernetRecoveryState previousState = ethernetRecoveryState;
  ethernetLinkUp = linkUp;

  if (linkUp) {
    ethernetRecoveryState = ETHERNET_RECOVERY_ONLINE;
    clearRecoverableNetworkError();
    setStatus(initialSample ? "Ethernet OK" : "Ethernet link recovered");
    setNetworkLedMode(fatalSafeStateActive ? LED_FAULT : LED_OK);

    if (!initialSample && (!previousLinkUp || previousState != ETHERNET_RECOVERY_ONLINE)) {
      ++ethernetRecoverySuccessCount;
      ethernetLastTransitionMs = millis();
      appendLogEvent("Ethernet link recovered; HTTP/SCPI available");
      Serial.print("Ethernet recovered at ");
      Serial.println(ipToString(eth.localIP()));
      Serial.flush();
    }
    return;
  }

  ethernetRecoveryState = ETHERNET_RECOVERY_WAIT_LINK;
  setRecoverableNetworkError(
    "Ethernet cable disconnected; waiting for link",
    "Ethernet link DOWN (recoverable)"
  );
  setNetworkLedMode(fatalSafeStateActive ? LED_FAULT : LED_BOOT);

  if (initialSample || previousLinkUp || previousState != ETHERNET_RECOVERY_WAIT_LINK) {
    if (!initialSample && previousLinkUp) {
      ++ethernetLinkDownCount;
    }
    ethernetLastTransitionMs = millis();
    closeScpiClientForLinkLoss();
    appendLogEvent("Ethernet link DOWN; auto-recovery waiting for cable");
    Serial.println("Ethernet link DOWN; HTTP/SCPI listeners retained; waiting for cable");
    Serial.flush();
  }
}

static void startNetworkServicesOnce() {
  if (ethernetServicesStarted) {
    return;
  }

  setupHttpServer();
  setupScpiServer();
  ethernetServicesStarted = true;
  appendLogEvent("Ethernet: HTTP and SCPI listeners started");
}

/**
 * @brief Start W5500/lwIP using the configured static IPv4 settings.
 *
 * The interface is deliberately started even when the cable is absent. This
 * allows lwIP and the TCP listeners to remain initialized so plugging the cable
 * in later does not require a board reset.
 *
 * @return true when the interface driver started.
 */
bool startEthernetStatic() {
  Serial.println();
  Serial.println("Ethernet: static direct mode with auto-recovery");
  Serial.flush();

  lwipPollingPeriod(20);
  eth.setSPISpeed(ETH_RUNTIME_SPI_HZ);

  if (!eth.config(
        DEVICE_IP,
        DEVICE_DNS,
        DEVICE_GATEWAY,
        DEVICE_SUBNET)) {
    ethernetInterfaceStarted = false;
    setRecoverableNetworkError(
      "Ethernet static configuration failed; retrying",
      "W5500/lwIP static config failed (recoverable retry)"
    );
    return false;
  }

  if (!eth.begin()) {
    ethernetInterfaceStarted = false;
    setRecoverableNetworkError(
      "Ethernet interface start failed; retrying",
      "W5500/lwIP begin failed (recoverable retry)"
    );
    return false;
  }

  ethernetInterfaceStarted = true;

  Serial.print("Ethernet interface configured IP: ");
  Serial.println(ipToString(DEVICE_IP));
  Serial.print("Ethernet active IP: ");
  Serial.println(ipToString(eth.localIP()));
  Serial.flush();

  return true;
}

static bool attemptEthernetInitialization() {
  ++ethernetRecoveryAttemptCount;
  ethernetLastRecoveryAttemptMs = millis();

  Serial.print("Ethernet initialization attempt ");
  Serial.println(ethernetRecoveryAttemptCount);
  Serial.flush();

  if (!w5500SoftwareResetAndProbe()) {
    ethernetInterfaceStarted = false;
    ethernetRecoveryState = ETHERNET_RECOVERY_HARDWARE_RETRY;
    setRecoverableNetworkError(
      "W5500 not detected; retrying automatically",
      "W5500 VERSION check failed (recoverable retry)"
    );
    setNetworkLedMode(LED_FAULT);
    if (ethernetRecoveryAttemptCount == 1U || (ethernetRecoveryAttemptCount % 12U) == 0U) {
      appendLogEvent("Ethernet: W5500 probe failed; retry scheduled");
    }
    return false;
  }

  if (!startEthernetStatic()) {
    ethernetRecoveryState = ETHERNET_RECOVERY_HARDWARE_RETRY;
    setNetworkLedMode(LED_FAULT);
    if (ethernetRecoveryAttemptCount == 1U || (ethernetRecoveryAttemptCount % 12U) == 0U) {
      appendLogEvent("Ethernet: interface start failed; retry scheduled");
    }
    return false;
  }

  startNetworkServicesOnce();

  bool linkUp = false;
  uint8_t phy = 0U;
  if (!w5500ReadLinkState(linkUp, &phy)) {
    ethernetRecoveryState = ETHERNET_RECOVERY_HARDWARE_RETRY;
    setRecoverableNetworkError(
      "W5500 status read failed; retrying",
      "W5500 status read failed after begin"
    );
    return false;
  }

  updateEthernetLinkState(linkUp, true);
  return true;
}

/**
 * @brief Initialize recoverable Ethernet operation during Core 0 setup.
 */
void ethernetAutoRecoveryBegin() {
  ethernetRecoveryState = ETHERNET_RECOVERY_UNINITIALIZED;
  ethernetLastProbeMs = 0U;
  ethernetLastRecoveryAttemptMs = 0U;
  ethernetLastTransitionMs = millis();
  attemptEthernetInitialization();
}

/**
 * @brief Poll W5500 PHY state and recover when a cable is connected later.
 *
 * If the W5500 was not available at boot, hardware initialization is retried
 * every ETH_HARDWARE_RETRY_INTERVAL_MS. Once lwIP is running, link transitions
 * do not recreate server objects or reset output hardware; listeners remain
 * active and a stale SCPI client is closed on disconnect.
 */
void serviceEthernetAutoRecovery() {
  const uint32_t nowMs = millis();

  if (!ethernetInterfaceStarted) {
    if (nowMs - ethernetLastRecoveryAttemptMs >= ETH_HARDWARE_RETRY_INTERVAL_MS) {
      attemptEthernetInitialization();
    }
    return;
  }

  if (nowMs - ethernetLastProbeMs < ETH_LINK_POLL_INTERVAL_MS) {
    return;
  }
  ethernetLastProbeMs = nowMs;

  bool linkUp = false;
  uint8_t phy = 0U;
  if (!w5500ReadLinkState(linkUp, &phy)) {
    ethernetRecoveryState = ETHERNET_RECOVERY_HARDWARE_RETRY;
    setRecoverableNetworkError(
      "W5500 communication lost",
      "W5500 status read failed after interface start"
    );
    setNetworkLedMode(LED_FAULT);
    closeScpiClientForLinkLoss();
    return;
  }

  if (linkUp != ethernetLinkUp ||
      (linkUp && ethernetRecoveryState != ETHERNET_RECOVERY_ONLINE) ||
      (!linkUp && ethernetRecoveryState != ETHERNET_RECOVERY_WAIT_LINK)) {
    updateEthernetLinkState(linkUp, false);
  }
}
