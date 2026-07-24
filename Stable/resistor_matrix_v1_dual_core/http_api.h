/** @file http_api.h @brief HTTP route, page, API and streaming interfaces. */
#pragma once
#include "utility_api.h"
#include "calibration_api.h"
#include "core_api.h"

// Maximum accepted size of a complete eight-channel calibration bundle.
// This limit is shared by the Files page and the import handler, so it must
// be visible to every HTTP service translation unit.
inline constexpr size_t CALIBRATION_BUNDLE_MAX_BYTES = 16384U;

class HttpResponseWriter {
 public:
  // Sized to keep the legacy /state response in one W5500/TCP write in the common case.
  // This removes the Gate 5 latency regression caused by many 512-byte chunks.
  static constexpr size_t BUFFER_SIZE = 4096;
  HttpResponseWriter();
  bool begin(int statusCode, const char* contentType, bool cacheable = false);
  bool write(const char* text);
  bool write(const String& text);
  bool writeChar(char value);
  bool printf(const char* format, ...);
  bool flush();
  void end();
  uint32_t bytesWritten() const { return bytesWritten_; }
  bool ok() const { return ok_; }
 private:
  static char buffer_[BUFFER_SIZE];
  size_t used_;
  uint32_t bytesWritten_;
  bool started_;
  bool ok_;
};

void appendCommonPageHeader(String& html, const char* title);
void appendCommonPageFooter(String& html);
void streamCommonPageHeader(HttpResponseWriter& out, const char* title);
void streamCommonPageFooter(HttpResponseWriter& out);
void streamCombinedChannelResistorTable(HttpResponseWriter& out);
void handleStaticCss();
void handleStaticJs();
void handlePing();
void handleState();
void handleApiV1Health();
void handleApiV1State();
void handleApiV1Channels();
void handleApiV1Diagnostics();
void handleApiV1CalibrationFiles();
void handleApiV1CalibrationDownload();
void handleApiV1CalibrationDownloadAll();
void handleApiV1ChannelSet();
void handleApiV1AllOff();
void handleApiV1ProfileApply();
void handleApiV1TargetApply();
void handleApiV1ToggleBit();
void handleApiV1IdentifyLed();
void handleMethodNotAllowed();
bool httpConfigurationOutputsAreOff();
bool httpGetLittleFsCapacity(uint64_t& totalBytes, uint64_t& usedBytes, uint64_t& freeBytes);
void httpSetFirmwareUpdateFsStatus(const char* prefix);
void handleLiveStatePage();
void handleRoot();
void appendNetworkSettingsForm(String& html);
void handleNetwork();
void handleNetworkSave();
void handleNetworkDelete();
void handleRuntimePage();
void handleFilesPage();
void handleFileDelete();
void handleSettings();
void handleUploadConfig();
void handleUploadConfigApi();
void handleDownloadConfig();
void handleCalibrationFilesApi();
void handleCalibrationDownloadAll();
void handleCalibrationDownloadAllFile();
void handleCalibrationImportAll();
void handleToggleBit();
void handleTargetApply();
void handleIdentifyLed();
void handleProfilesPage();
void handleProfileSave();
void handleProfileApply();
void handleProfileDelete();
void handleSafetyPage();
void handleSafetySave();
void handleScpiPage();
void handleLogPage();
void handleLogDownload();
void handleBackupPage();
void handleBackupDownload();
void deleteFilesByPrefix(const char* prefix);
void handleFactoryReset();
void handleWatchdogPage();
void handleCalibrationMetaSave();
void handleCalibrationMetaDelete();
void handleSet();
void handleAllOff();
void handleFirmwarePage();
void handleFirmwareUpdateDone();
void handleFirmwareUpdateUpload();
void handleNotFound();
void setupHttpServer();
