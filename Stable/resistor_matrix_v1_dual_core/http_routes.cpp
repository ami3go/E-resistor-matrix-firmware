/**
 * @file http_routes.cpp
 * @brief Gate 5 route registry, API versioning and HTTP method policy.
 */
#include "http_api.h"

void handleMethodNotAllowed() {
  noteHttpRequest();
  ++httpMethodRejectedCount;
  server.sendHeader("Allow", "POST");
  server.sendHeader("Deprecation", "true");
  server.sendHeader("Sunset", "Wed, 31 Dec 2026 23:59:59 GMT");
  server.send(405, "application/json",
              "{\"error\":\"method_not_allowed\",\"required_method\":\"POST\",\"api\":\"/api/v1/\"}\n");
}

void handleNotFound() {
  noteHttpRequest();
  String message = "Not found: ";
  message += server.uri();
  message += "\nUse /api/v1/health or open / for the browser UI.\n";
  sendNoCacheHeaders();
  server.send(404, "text/plain", message);
}

void setupHttpServer() {
  // Flash-backed static resources.
  server.on("/assets/app.css", HTTP_GET, handleStaticCss);
  server.on("/assets/app.js", HTTP_GET, handleStaticJs);

  // Browser pages and read-only compatibility endpoints.
  server.on("/", HTTP_GET, handleRoot);
  server.on("/ping", HTTP_GET, handlePing);
  server.on("/state", HTTP_GET, handleState);
  server.on("/live", HTTP_GET, handleLiveStatePage);
  server.on("/settings", HTTP_GET, handleSettings);
  server.on("/profiles", HTTP_GET, handleProfilesPage);
  server.on("/safety", HTTP_GET, handleSafetyPage);
  server.on("/network", HTTP_GET, handleNetwork);
  server.on("/runtime", HTTP_GET, handleRuntimePage);
  server.on("/scpi", HTTP_GET, handleScpiPage);
  server.on("/files", HTTP_GET, handleFilesPage);
  server.on("/log", HTTP_GET, handleLogPage);
  server.on("/log_download", HTTP_GET, handleLogDownload);
  server.on("/backup", HTTP_GET, handleBackupPage);
  server.on("/backup_download", HTTP_GET, handleBackupDownload);
  server.on("/watchdog", HTTP_GET, handleWatchdogPage);
  server.on("/firmware", HTTP_GET, handleFirmwarePage);
  server.on("/config", HTTP_GET, handleDownloadConfig);

  // State-changing compatibility paths now require POST.
  server.on("/set", HTTP_POST, handleSet);
  server.on("/toggle_bit", HTTP_POST, handleToggleBit);
  server.on("/alloff", HTTP_POST, handleAllOff);
  server.on("/target_apply", HTTP_POST, handleTargetApply);
  server.on("/identify_led", HTTP_POST, handleIdentifyLed);
  server.on("/profile_save", HTTP_POST, handleProfileSave);
  server.on("/profile_apply", HTTP_POST, handleProfileApply);
  server.on("/profile_delete", HTTP_POST, handleProfileDelete);
  server.on("/factory_reset", HTTP_POST, handleFactoryReset);
  server.on("/safety_save", HTTP_POST, handleSafetySave);
  server.on("/network_save", HTTP_POST, handleNetworkSave);
  server.on("/network_delete", HTTP_POST, handleNetworkDelete);
  server.on("/file_delete", HTTP_POST, handleFileDelete);
  server.on("/meta_save", HTTP_POST, handleCalibrationMetaSave);
  server.on("/meta_delete", HTTP_POST, handleCalibrationMetaDelete);
  server.on("/upload_config", HTTP_POST, handleUploadConfig);
  server.on("/calibration_import_all", HTTP_POST, handleCalibrationImportAll);
  server.on("/firmware_update", HTTP_POST, handleFirmwareUpdateDone, handleFirmwareUpdateUpload);

  // Explicit 405 responses prove that browser crawlers and stale bookmarks can
  // no longer mutate output state through GET.
  server.on("/set", HTTP_GET, handleMethodNotAllowed);
  server.on("/toggle_bit", HTTP_GET, handleMethodNotAllowed);
  server.on("/alloff", HTTP_GET, handleMethodNotAllowed);
  server.on("/target_apply", HTTP_GET, handleMethodNotAllowed);
  server.on("/identify_led", HTTP_GET, handleMethodNotAllowed);
  server.on("/profile_apply", HTTP_GET, handleMethodNotAllowed);
  server.on("/profile_delete", HTTP_GET, handleMethodNotAllowed);
  server.on("/factory_reset", HTTP_GET, handleMethodNotAllowed);
  server.on("/profile_save", HTTP_GET, handleMethodNotAllowed);
  server.on("/safety_save", HTTP_GET, handleMethodNotAllowed);
  server.on("/network_save", HTTP_GET, handleMethodNotAllowed);
  server.on("/network_delete", HTTP_GET, handleMethodNotAllowed);
  server.on("/file_delete", HTTP_GET, handleMethodNotAllowed);
  server.on("/meta_save", HTTP_GET, handleMethodNotAllowed);
  server.on("/meta_delete", HTTP_GET, handleMethodNotAllowed);
  server.on("/upload_config", HTTP_GET, handleMethodNotAllowed);
  server.on("/calibration_import_all", HTTP_GET, handleMethodNotAllowed);
  server.on("/firmware_update", HTTP_GET, handleMethodNotAllowed);
  server.on("/api/upload_config", HTTP_GET, handleMethodNotAllowed);
  server.on("/api/calibration/upload", HTTP_GET, handleMethodNotAllowed);

  // Canonical versioned API.
  server.on("/api/v1/health", HTTP_GET, handleApiV1Health);
  server.on("/api/v1/state", HTTP_GET, handleApiV1State);
  server.on("/api/v1/channels", HTTP_GET, handleApiV1Channels);
  server.on("/api/v1/diagnostics", HTTP_GET, handleApiV1Diagnostics);
  server.on("/api/v1/calibration/files", HTTP_GET, handleApiV1CalibrationFiles);
  server.on("/api/v1/calibration/download", HTTP_GET, handleApiV1CalibrationDownload);
  server.on("/api/v1/calibration/download_all", HTTP_GET, handleApiV1CalibrationDownloadAll);
  server.on("/api/v1/control/channel", HTTP_POST, handleApiV1ChannelSet);
  server.on("/api/v1/control/all_off", HTTP_POST, handleApiV1AllOff);
  server.on("/api/v1/control/profile", HTTP_POST, handleApiV1ProfileApply);
  server.on("/api/v1/control/target", HTTP_POST, handleApiV1TargetApply);
  server.on("/api/v1/control/toggle", HTTP_POST, handleApiV1ToggleBit);
  server.on("/api/v1/identify", HTTP_POST, handleApiV1IdentifyLed);

  // Legacy API aliases retained for one deprecation period.
  server.on("/api/upload_config", HTTP_POST, handleUploadConfigApi);
  server.on("/api/calibration/upload", HTTP_POST, handleUploadConfigApi);
  server.on("/api/calibration/files", HTTP_GET, handleCalibrationFilesApi);
  server.on("/api/calibration/download", HTTP_GET, handleDownloadConfig);
  server.on("/api/calibration/download_all", HTTP_GET, handleCalibrationDownloadAll);
  server.on("/calibration_download_all", HTTP_GET, handleCalibrationDownloadAllFile);

  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started on port 80 (API v1)");
  Serial.flush();
}
