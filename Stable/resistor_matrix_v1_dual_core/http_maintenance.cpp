/** @file http_maintenance.cpp @brief Gate 5 split HTTP service module. */
#include "http_api.h"
static File s_firmwareUploadFile;
static constexpr const char* FIRMWARE_UPLOAD_TMP_PATH = "/fw_update.bin.tmp";

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

  appendLogEvent("Ethernet settings saved");

  server.sendHeader("Location", "/network", true);
  server.send(303, "text/plain", "See Other\n");
}

void handleNetworkDelete() {
  noteHttpRequest();

  if (!littleFsReady) {
    server.send(500, "text/plain", "LittleFS not ready\n");
    return;
  }

  LittleFS.remove(networkConfigPath());
  DEVICE_IP = IPAddress(DEFAULT_DEVICE_IP_OCTETS[0], DEFAULT_DEVICE_IP_OCTETS[1], DEFAULT_DEVICE_IP_OCTETS[2], DEFAULT_DEVICE_IP_OCTETS[3]);
  DEVICE_SUBNET = IPAddress(255, 255, 255, 0);
  DEVICE_GATEWAY = IPAddress(0, 0, 0, 0);
  DEVICE_DNS = IPAddress(0, 0, 0, 0);

  appendLogEvent("Ethernet settings file deleted");
  setStatus("Ethernet settings deleted. Restart recommended.");

  server.sendHeader("Location", "/network", true);
  server.send(303, "text/plain", "See Other\n");
}

void handleFirmwareUpdateDone() {
  noteHttpRequest();

  String html;
  html.reserve(3000);
  appendCommonPageHeader(html, "Firmware Update Result");
  html += "<h1>Firmware Update</h1>";

  if (firmwareUpdateSucceeded) {
    html += "<div class='card'><h2 class='ok'>Update accepted</h2>";
    html += "<p>The firmware image was written successfully. The board will reboot now.</p>";
    html += "<p class='small'>Refresh the page after the board comes back online.</p></div>";
  } else {
    html += "<div class='card'><h2 class='danger'>Update failed</h2><pre><code>";
    html += firmwareUpdateStatus;
    html += "</code></pre>";
    html += "<p><a href='/firmware'>Return to firmware page</a></p></div>";
  }

  appendCommonPageFooter(html);
  sendNoCacheHeaders();
  server.send(firmwareUpdateSucceeded ? 200 : 500, "text/html", html);

  if (firmwareUpdateSucceeded) {
    delay(500);
    rp2040.reboot();
  }
}

void handleFirmwareUpdateUpload() {
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    firmwareUpdateInProgress = true;
    firmwareUpdateSucceeded = false;
    snprintf(firmwareUpdateStatus, sizeof(firmwareUpdateStatus), "Starting firmware upload: %s", upload.filename.c_str());
    appendLogEvent("Firmware update: upload started");

    char allOffReason[128] = {0};
    if (!forceAllOff(allOffReason, sizeof(allOffReason))) {
      snprintf(firmwareUpdateStatus, sizeof(firmwareUpdateStatus),
               "Rejected: could not confirm all outputs OFF: %s",
               allOffReason[0] ? allOffReason : "unknown all-off failure");
      appendLogEvent(firmwareUpdateStatus);
      firmwareUpdateInProgress = false;
      return;
    }
    delay(100);

    if (!littleFsReady) {
      httpSetFirmwareUpdateFsStatus("Rejected: LittleFS is not ready.");
      firmwareUpdateInProgress = false;
      return;
    }

    uint64_t otaTotalBytes = 0;
    uint64_t otaUsedBytes = 0;
    uint64_t otaFreeBytes = 0;
    if (!httpGetLittleFsCapacity(otaTotalBytes, otaUsedBytes, otaFreeBytes) || otaTotalBytes == 0 || otaFreeBytes < 4096ULL) {
      httpSetFirmwareUpdateFsStatus("Rejected: not enough LittleFS space for OTA staging.");
      firmwareUpdateInProgress = false;
      return;
    }

    String lowerName = upload.filename;
    lowerName.toLowerCase();
    if (!lowerName.endsWith(".bin") && !lowerName.endsWith(".bin.gz") && !lowerName.endsWith(".bin.signed") && !lowerName.endsWith(".bin.gz.signed")) {
      snprintf(firmwareUpdateStatus, sizeof(firmwareUpdateStatus), "Rejected: firmware update requires .bin/.bin.gz/.bin.signed, got %s", upload.filename.c_str());
      firmwareUpdateInProgress = false;
      return;
    }

    if (s_firmwareUploadFile) {
      s_firmwareUploadFile.close();
    }
    LittleFS.remove(FIRMWARE_UPLOAD_TMP_PATH);
    s_firmwareUploadFile = LittleFS.open(FIRMWARE_UPLOAD_TMP_PATH, "w");
    if (!s_firmwareUploadFile) {
      httpSetFirmwareUpdateFsStatus("Rejected: cannot create temporary firmware file in LittleFS.");
      firmwareUpdateInProgress = false;
      return;
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (!firmwareUpdateInProgress || !s_firmwareUploadFile) {
      return;
    }

    size_t written = s_firmwareUploadFile.write(upload.buf, upload.currentSize);
    if (written != upload.currentSize) {
      uint64_t otaTotalBytes = 0;
      uint64_t otaUsedBytes = 0;
      uint64_t otaFreeBytes = 0;
      if (httpGetLittleFsCapacity(otaTotalBytes, otaUsedBytes, otaFreeBytes)) {
        snprintf(firmwareUpdateStatus, sizeof(firmwareUpdateStatus),
                 "LittleFS staging short write: %u/%u. LittleFS total=%llu used=%llu free=%llu bytes. The uploaded .bin must fit in FS; select a Flash Size with enough FS, then flash once by USB.",
                 unsigned(written), unsigned(upload.currentSize),
                 (unsigned long long)otaTotalBytes,
                 (unsigned long long)otaUsedBytes,
                 (unsigned long long)otaFreeBytes);
      } else {
        snprintf(firmwareUpdateStatus, sizeof(firmwareUpdateStatus),
                 "LittleFS staging short write: %u/%u. LittleFS capacity unavailable; select a Flash Size with FS. No-FS layouts cannot use web .bin update.",
                 unsigned(written), unsigned(upload.currentSize));
      }
      s_firmwareUploadFile.close();
      LittleFS.remove(FIRMWARE_UPLOAD_TMP_PATH);
      firmwareUpdateInProgress = false;
      firmwareUpdateSucceeded = false;
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (s_firmwareUploadFile) {
      s_firmwareUploadFile.close();
    }

    if (!firmwareUpdateInProgress) {
      return;
    }

    File fw = LittleFS.open(FIRMWARE_UPLOAD_TMP_PATH, "r");
    if (!fw) {
      snprintf(firmwareUpdateStatus, sizeof(firmwareUpdateStatus), "Update failed: cannot reopen staged firmware file");
      firmwareUpdateSucceeded = false;
      firmwareUpdateInProgress = false;
      appendLogEvent("Firmware update: failed opening staged file");
      return;
    }

    size_t firmwareSize = fw.size();
    if (firmwareSize == 0) {
      fw.close();
      LittleFS.remove(FIRMWARE_UPLOAD_TMP_PATH);
      snprintf(firmwareUpdateStatus, sizeof(firmwareUpdateStatus), "Update failed: uploaded firmware file is empty");
      firmwareUpdateSucceeded = false;
      firmwareUpdateInProgress = false;
      return;
    }

    if (!Update.begin(firmwareSize)) {
      fw.close();
      LittleFS.remove(FIRMWARE_UPLOAD_TMP_PATH);
      snprintf(firmwareUpdateStatus, sizeof(firmwareUpdateStatus), "Update.begin(%u) failed", unsigned(firmwareSize));
      Update.printError(Serial);
      firmwareUpdateSucceeded = false;
      firmwareUpdateInProgress = false;
      appendLogEvent("Firmware update: Update.begin failed");
      return;
    }

    size_t written = Update.writeStream(fw);
    fw.close();

    if (written != firmwareSize) {
      // Arduino-Pico UpdaterClass does not provide Update.abort().
      // Do not call Update.end() after an incomplete write; remove the staged
      // file, mark failure, and allow a fresh upload attempt after reload/reboot.
      LittleFS.remove(FIRMWARE_UPLOAD_TMP_PATH);
      snprintf(firmwareUpdateStatus, sizeof(firmwareUpdateStatus), "Update.writeStream short write: %u/%u", unsigned(written), unsigned(firmwareSize));
      Update.printError(Serial);
      firmwareUpdateSucceeded = false;
      firmwareUpdateInProgress = false;
      appendLogEvent("Firmware update: writeStream failed");
      return;
    }

    if (Update.end()) {
      firmwareUpdateSucceeded = true;
      snprintf(firmwareUpdateStatus, sizeof(firmwareUpdateStatus), "Firmware update OK, %u bytes staged; rebooting", unsigned(firmwareSize));
      appendLogEvent("Firmware update: successful, rebooting");
    } else {
      firmwareUpdateSucceeded = false;
      snprintf(firmwareUpdateStatus, sizeof(firmwareUpdateStatus), "Update.end() failed after %u bytes", unsigned(firmwareSize));
      Update.printError(Serial);
      appendLogEvent("Firmware update: failed at Update.end");
    }

    LittleFS.remove(FIRMWARE_UPLOAD_TMP_PATH);
    firmwareUpdateInProgress = false;
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    if (s_firmwareUploadFile) {
      s_firmwareUploadFile.close();
    }
    // Arduino-Pico UpdaterClass has no abort() method. At this stage the
    // firmware image is only being staged into LittleFS, so cleanup is simply
    // removing the temporary upload file and clearing the state flags.
    LittleFS.remove(FIRMWARE_UPLOAD_TMP_PATH);
    firmwareUpdateInProgress = false;
    firmwareUpdateSucceeded = false;
    snprintf(firmwareUpdateStatus, sizeof(firmwareUpdateStatus), "Firmware upload aborted");
    appendLogEvent("Firmware update: aborted");
  }
}

void handleFileDelete() {
  noteHttpRequest();

  if (!littleFsReady) {
    server.send(500, "text/plain", "LittleFS not ready\n");
    return;
  }

  if (!server.hasArg("path")) {
    server.send(400, "text/plain", "Missing file path\n");
    return;
  }

  String path = server.arg("path");
  path.trim();
  if (!path.startsWith("/")) {
    path = "/" + path;
  }

  if (path.length() < 2 || path.indexOf("..") >= 0 || path.indexOf('/', 1) >= 0) {
    server.send(400, "text/plain", "Only LittleFS root files can be deleted from this page\n");
    return;
  }

  if (path == FIRMWARE_UPLOAD_TMP_PATH) {
    server.send(400, "text/plain", "Refusing to delete active firmware update temporary file\n");
    return;
  }

  if (!LittleFS.exists(path)) {
    server.send(404, "text/plain", "File not found\n");
    return;
  }

  bool ok = LittleFS.remove(path);
  if (ok) {
    char msg[128];
    snprintf(msg, sizeof(msg), "LittleFS file deleted: %s", path.c_str());
    appendLogEvent(msg);
    setStatus("LittleFS file deleted");
  } else {
    setLastError("LittleFS file delete failed");
  }

  server.sendHeader("Location", "/files", true);
  server.send(303, "text/plain", "See Other\n");
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

  if (!httpConfigurationOutputsAreOff()) {
    server.send(409, "text/plain", "Turn all channels OFF before changing calibration data.\n");
    return;
  }

  if (!server.hasArg("config")) {
    server.send(400, "text/plain", "Missing config text\n");
    return;
  }

  float previousValues[BIT_COUNT];
  for (uint8_t bit = 0; bit < BIT_COUNT; ++bit) previousValues[bit] = channelResistorOhms[ch][bit];
  String configText = server.arg("config");
  char error[96];

  if (!parseChannelConfigText(ch, configText, error, sizeof(error))) {
    setStatus(error);
    setLastError(error);
    server.send(400, "text/plain", String("Config parse failed: ") + error + "\n");
    return;
  }

  char policyReason[128] = {0};
  if (!installCore1PolicySnapshot(policyReason, sizeof(policyReason))) {
    for (uint8_t bit = 0; bit < BIT_COUNT; ++bit) channelResistorOhms[ch][bit] = previousValues[bit];
    rebuildConductanceCache();
    installCore1PolicySnapshot(nullptr, 0);
    setLastError(policyReason);
    server.send(500, "text/plain", String("Core 1 policy installation failed: ") + policyReason + "\n");
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
  appendLogEvent(msg);

  redirectToSettings();
}

void handleUploadConfigApi() {
  noteHttpRequest();

  Serial.println("HTTP API calibration upload");
  Serial.flush();

  uint8_t ch = 0;
  if (!parseChannel(ch)) {
    server.send(400, "text/plain", "ERR,bad_channel\n");
    return;
  }

  if (!httpConfigurationOutputsAreOff()) {
    server.send(409, "text/plain", "ERR,outputs_must_be_off\n");
    return;
  }

  float previousValues[BIT_COUNT];
  for (uint8_t bit = 0; bit < BIT_COUNT; ++bit) previousValues[bit] = channelResistorOhms[ch][bit];
  String configText;

  if (server.hasArg("config")) {
    configText = server.arg("config");
  } else if (server.hasArg("plain")) {
    configText = server.arg("plain");
  } else {
    server.send(400, "text/plain", "ERR,missing_config\n");
    return;
  }

  char error[96];

  if (!parseChannelConfigText(ch, configText, error, sizeof(error))) {
    setStatus(error);
    setLastError(error);
    server.send(400, "text/plain", String("ERR,parse,") + error + "\n");
    return;
  }

  char policyReason[128] = {0};
  if (!installCore1PolicySnapshot(policyReason, sizeof(policyReason))) {
    for (uint8_t bit = 0; bit < BIT_COUNT; ++bit) channelResistorOhms[ch][bit] = previousValues[bit];
    rebuildConductanceCache();
    installCore1PolicySnapshot(nullptr, 0);
    setLastError(policyReason);
    server.send(500, "text/plain", String("ERR,policy,") + policyReason + "\n");
    return;
  }

  bool saved = saveChannelConfigToLittleFS(ch);

  char msg[128];
  snprintf(
    msg,
    sizeof(msg),
    "CH%u calibration uploaded by API%s",
    unsigned(ch + 1),
    saved ? " and saved" : " to RAM only"
  );

  setStatus(msg);
  clearLastError();
  appendLogEvent(msg);

  Serial.println(msg);
  Serial.flush();

  String response;
  response.reserve(220);
  response += "OK\n";
  response += "channel=";
  response += String(ch + 1);
  response += "\n";
  response += "saved=";
  response += saved ? "1" : "0";
  response += "\n";
  response += "serial=";
  response += deviceSerialNumber;
  response += "\n";
  response += "path=";
  response += channelConfigPath(ch);
  response += "\n";

  server.send(200, "text/plain", response);
}



void handleCalibrationImportAll() {
  noteHttpRequest();

  if (!littleFsReady) {
    server.send(503, "text/plain", "LittleFS is not ready; calibration restore requires persistent storage.\n");
    return;
  }

  refreshCore0OutputMirror();
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    if (channelMask[ch] != 0) {
      server.send(409, "text/plain", "Turn all channels OFF before importing calibration data.\n");
      return;
    }
  }

  if (!server.hasArg("bundle")) {
    server.send(400, "text/plain", "Missing calibration bundle.\n");
    return;
  }

  String bundle = server.arg("bundle");
  if (bundle.length() == 0 || bundle.length() > CALIBRATION_BUNDLE_MAX_BYTES) {
    server.send(400, "text/plain", "Calibration bundle is empty or exceeds 16384 bytes.\n");
    return;
  }

  static float previousTables[CHANNEL_COUNT][BIT_COUNT];
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
    for (uint8_t bit = 0; bit < BIT_COUNT; ++bit) previousTables[ch][bit] = channelResistorOhms[ch][bit];
  }

  char error[160];
  if (!restoreAllChannelConfigsFromBundleText(bundle, error, sizeof(error))) {
    setStatus("Calibration bundle import failed");
    setLastError(error);
    appendLogEvent(error);
    server.send(400, "text/plain", String("Calibration import failed: ") + error + "\n");
    return;
  }

  char policyReason[160] = {0};
  if (!installCore1PolicySnapshot(policyReason, sizeof(policyReason))) {
    for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
      for (uint8_t bit = 0; bit < BIT_COUNT; ++bit) channelResistorOhms[ch][bit] = previousTables[ch][bit];
      saveChannelConfigToLittleFS(ch);
    }
    rebuildConductanceCache();
    installCore1PolicySnapshot(nullptr, 0);
    setLastError(policyReason);
    server.send(500, "text/plain", String("Core 1 policy installation failed: ") + policyReason + "\n");
    return;
  }

  setStatus("All channel calibration tables imported");
  clearLastError();
  appendLogEvent("All eight calibration tables imported and saved");
  server.sendHeader("Location", "/settings?cal_import=ok", true);
  server.send(303, "text/plain", "See Other\n");
}

void handleLogDownload() {
  noteHttpRequest();

  String filename = String("e_resistor_log_") + deviceSerialNumber + ".txt";
  server.sendHeader("Content-Disposition", String("attachment; filename=\"") + filename + "\"");
  server.sendHeader("X-Content-Type-Options", "nosniff");

  RuntimeStateSnapshot snapshot{};
  captureRuntimeStateSnapshot(snapshot);
  HttpResponseWriter out;
  if (!out.begin(200, "text/plain; charset=utf-8")) return;
  out.write("E-Resistor event log\n====================\n");
  out.printf("Board serial: %s\nFirmware version: %s\n", snapshot.serial, FIRMWARE_VERSION);
  out.printf("Firmware build: %s %s\nExport uptime: %lu s\n", FIRMWARE_BUILD_DATE, FIRMWARE_BUILD_TIME,
             static_cast<unsigned long>(snapshot.uptimeMs / 1000UL));
  out.printf("Status: %s\nLast error: %s\n", snapshot.status, snapshot.error);
  out.printf("Core 1 engine: %s\nOutputs known safe: %s\nLittleFS: %s\nEthernet fault: %s\nEthernet state: %s\nEthernet link: %s\nIP address: %s\n\n",
             snapshot.core1EngineReady ? "ready" : "not ready",
             snapshot.outputsKnownSafe ? "yes" : "no",
             snapshot.littleFsReady ? "ready" : "not ready",
             snapshot.ethernetFault ? "yes" : "no",
             ethernetRecoveryStateText(snapshot.ethernetRecoveryState),
             snapshot.ethernetLinkUp ? "up" : "down", snapshot.ip);
  out.write("Event history\n-------------\n");
  for (uint8_t i = 0; i < eventLogCount; ++i) {
    const uint8_t index = (eventLogHead + 32U - eventLogCount + i) % 32U;
    out.write(eventLog[index]);
    out.writeChar('\n');
  }
  if (eventLogCount == 0U) out.write("No events yet.\n");
  out.end();
}

void handleBackupDownload() {
  noteHttpRequest();

  String filename = String("e_resistor_backup_") + deviceSerialNumber + ".txt";
  server.sendHeader("Content-Disposition", String("attachment; filename=\"") + filename + "\"");
  server.sendHeader("X-Content-Type-Options", "nosniff");

  HttpResponseWriter out;
  if (!out.begin(200, "text/plain; charset=utf-8")) return;
  out.write("# E-Resistor backup\n# format=2\n# Serial number: ");
  out.write(deviceSerialNumber);
  out.printf("\n# Firmware version: %s\n# Generated at uptime ms: %lu\n\n[network]\n",
             FIRMWARE_VERSION, static_cast<unsigned long>(millis()));
  const String network = networkConfigToText();
  out.write(network);
  out.write("\n[safety]\nversion,2\n");
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
    out.printf("ch%u_min_ohm,%.6f\nch%u_max_ohm,%.6f\nch%u_max_active_bits,%u\n",
               unsigned(ch + 1), safetyMinOhm[ch], unsigned(ch + 1), safetyMaxOhm[ch],
               unsigned(ch + 1), unsigned(safetyMaxActiveBits[ch]));
  }
  out.printf("expert_mode,%u\n", safetyExpertMode ? 1U : 0U);

  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
    out.printf("\n[ch%u_config]\nbit,mosfet_name,nominal_resistance\n", unsigned(ch + 1));
    for (uint8_t bit = 0; bit < BIT_COUNT; ++bit) {
      const float resistance = getRuntimeResistanceOhms(ch, bit);
      if (isfinite(resistance) && resistance > 0.0f) {
        out.printf("%u,%s,%.6f\n", unsigned(bit), mosfetNameForBit(bit), double(resistance));
      } else {
        out.printf("%u,%s,NaN\n", unsigned(bit), mosfetNameForBit(bit));
      }
    }
    out.flush();
  }
  out.end();
}

void deleteFilesByPrefix(const char* prefix) {
  if (!littleFsReady) {
    return;
  }

  Dir dir = LittleFS.openDir("/");
  while (dir.next()) {
    String name = dir.fileName();
    if (!name.startsWith("/")) {
      name = "/" + name;
    }
    if (name.startsWith(prefix)) {
      LittleFS.remove(name);
    }
  }
}

void handleFactoryReset() {
  noteHttpRequest();

  String scope = server.hasArg("scope") ? server.arg("scope") : "";
  scope.toLowerCase();

  if (scope == "calibration") {
    deleteFilesByPrefix("/ch");
    deleteFilesByPrefix("/meta_ch");
    appendLogEvent("Factory reset: calibration files deleted");
  } else if (scope == "profiles") {
    deleteFilesByPrefix("/profile_");
    appendLogEvent("Factory reset: profiles deleted");
  } else if (scope == "network") {
    if (littleFsReady) {
      LittleFS.remove(networkConfigPath());
    }
    appendLogEvent("Factory reset: network config deleted");
  } else if (scope == "all") {
    deleteFilesByPrefix("/ch");
    deleteFilesByPrefix("/meta_ch");
    deleteFilesByPrefix("/profile_");
    if (littleFsReady) {
      LittleFS.remove(networkConfigPath());
      LittleFS.remove(safetyConfigPath());
    }
    appendLogEvent("Factory reset: all known config files deleted");
  } else {
    server.send(400, "text/plain", "Bad reset scope\n");
    return;
  }

  setStatus("Factory reset action complete. Restart recommended.");
  server.sendHeader("Location", "/files", true);
  server.send(303, "text/plain", "See Other\n");
}

void handleCalibrationMetaSave() {
  noteHttpRequest();

  uint8_t ch = 0;
  if (!parseChannel(ch)) {
    server.send(400, "text/plain", "Bad channel\n");
    return;
  }

  if (!littleFsReady) {
    server.send(500, "text/plain", "LittleFS not ready\n");
    return;
  }

  File f = LittleFS.open(calibrationMetaPath(ch), "w");
  if (!f) {
    server.send(500, "text/plain", "Failed to write metadata\n");
    return;
  }

  f.print("date,");
  f.println(server.hasArg("date") ? server.arg("date") : "");
  f.print("valid_until,");
  f.println(server.hasArg("valid_until") ? server.arg("valid_until") : "");
  f.print("operator,");
  f.println(server.hasArg("operator") ? server.arg("operator") : "");
  f.print("dmm,");
  f.println(server.hasArg("dmm") ? server.arg("dmm") : "");
  f.print("report_id,");
  f.println(server.hasArg("report_id") ? server.arg("report_id") : "");
  f.close();

  appendLogEvent("Calibration metadata saved");
  setStatus("Calibration metadata saved");

  server.sendHeader("Location", "/settings", true);
  server.send(303, "text/plain", "See Other\n");
}

void handleCalibrationMetaDelete() {
  noteHttpRequest();

  uint8_t ch = 0;
  if (!parseChannel(ch)) {
    server.send(400, "text/plain", "Bad channel\n");
    return;
  }

  if (!littleFsReady) {
    server.send(500, "text/plain", "LittleFS not ready\n");
    return;
  }

  String path = calibrationMetaPath(ch);
  bool existed = LittleFS.exists(path);
  LittleFS.remove(path);

  char msg[128];
  snprintf(msg, sizeof(msg), "Calibration metadata %s for CH%u", existed ? "deleted" : "not present", unsigned(ch + 1));
  appendLogEvent(msg);
  setStatus("Calibration metadata delete complete");

  server.sendHeader("Location", String("/settings?ch=") + String(ch + 1), true);
  server.send(303, "text/plain", "See Other\n");
}

