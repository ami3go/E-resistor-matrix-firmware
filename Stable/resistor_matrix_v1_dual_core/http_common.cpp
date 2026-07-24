/**
 * @file http_common.cpp
 * @brief Shared HTTP safety and LittleFS diagnostics without service-specific ownership.
 */
#include "http_api.h"

bool httpGetLittleFsCapacity(uint64_t& totalBytes, uint64_t& usedBytes, uint64_t& freeBytes) {
  totalBytes = 0;
  usedBytes = 0;
  freeBytes = 0;
  if (!littleFsReady) return false;
  FSInfo fsInfo;
  if (!LittleFS.info(fsInfo)) return false;
  totalBytes = fsInfo.totalBytes;
  usedBytes = fsInfo.usedBytes;
  if (totalBytes >= usedBytes) freeBytes = totalBytes - usedBytes;
  return true;
}

void httpSetFirmwareUpdateFsStatus(const char* prefix) {
  uint64_t totalBytes = 0;
  uint64_t usedBytes = 0;
  uint64_t freeBytes = 0;
  if (httpGetLittleFsCapacity(totalBytes, usedBytes, freeBytes)) {
    snprintf(firmwareUpdateStatus, sizeof(firmwareUpdateStatus),
             "%s LittleFS total=%llu used=%llu free=%llu bytes. OTA .bin staging requires sufficient free space.",
             prefix,
             static_cast<unsigned long long>(totalBytes),
             static_cast<unsigned long long>(usedBytes),
             static_cast<unsigned long long>(freeBytes));
  } else {
    snprintf(firmwareUpdateStatus, sizeof(firmwareUpdateStatus),
             "%s LittleFS capacity unavailable; OTA staging requires a mounted filesystem.",
             prefix);
  }
}

bool httpConfigurationOutputsAreOff() {
  CoreOutputSnapshot snapshot{};
  if (readCoreOutputSnapshot(snapshot)) {
    for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
      if (snapshot.masks[ch] != 0U) return false;
    }
    return true;
  }
  refreshCore0OutputMirror();
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
    if (channelMask[ch] != 0U) return false;
  }
  return true;
}
