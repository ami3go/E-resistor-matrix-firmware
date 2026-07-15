/**
 * @file littlefs_page_helpers.cpp
 * @brief LittleFS storage-reporting and file-list rendering helpers.
 */

#include "app.h"

// 10_littlefs_page_helpers.ino
// Split from rp2040_w5500_resistor_matrix_v1_safety_logic.ino.
// Keep all files in the same Arduino sketch folder.

// ============================================================
// LittleFS Settings page helpers
// ============================================================

/**
 * @brief U64 To String.
 * @param value Function parameter.
 * @return Result value; for bool, true means the operation succeeded.
 */
String u64ToString(uint64_t value) {
  char buf[32];
  snprintf(buf, sizeof(buf), "%llu", (unsigned long long)value);
  return String(buf);
}

/**
 * @brief Format Bytes Human.
 * @param bytes Function parameter.
 * @return Result value; for bool, true means the operation succeeded.
 */
String formatBytesHuman(uint64_t bytes) {
  char buf[48];

  if (bytes >= 1024ULL * 1024ULL) {
    double mb = double(bytes) / double(1024ULL * 1024ULL);
    snprintf(buf, sizeof(buf), "%llu bytes (%.2f MiB)", (unsigned long long)bytes, mb);
  } else if (bytes >= 1024ULL) {
    double kb = double(bytes) / 1024.0;
    snprintf(buf, sizeof(buf), "%llu bytes (%.2f KiB)", (unsigned long long)bytes, kb);
  } else {
    snprintf(buf, sizeof(buf), "%llu bytes", (unsigned long long)bytes);
  }

  return String(buf);
}

/**
 * @brief Return a user-facing category for a LittleFS root file.
 */
static String littleFsFileType(const String& path) {
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    if (path == channelConfigPath(ch)) return "Channel calibration";
  }
  if (path.startsWith("/meta_ch")) return "Calibration metadata";
  if (path.startsWith("/profile_")) return "Profile";
  if (path == networkConfigPath()) return "Ethernet settings";
  if (path == safetyConfigPath()) return "Safety settings";
  if (path == "/fw_update.bin.tmp") return "Firmware staging";
  return "Other";
}

/**
 * @brief Append one delete action cell for an existing LittleFS file.
 */
static void appendLittleFsDeleteAction(String& html, const String& fullPath) {
  html += "<form method='POST' action='/file_delete' onsubmit=\"return confirm('Delete this LittleFS file?');\" style='margin:0'>";
  html += "<input type='hidden' name='path' value='";
  html += fullPath;
  html += "'>";
  html += "<button class='off' type='submit'>Delete</button>";
  html += "</form>";
}

/**
 * @brief Append one combined table containing expected channel calibration
 *        files and every other stored file in the LittleFS root.
 */
void appendLittleFsCombinedFiles(String& html) {
  html += "<h3>LittleFS file inventory</h3>";
  html += "<p class='small'>Expected CH1-CH8 calibration files and all other root files are shown in one table. Existing calibration files appear only once.</p>";
  html += "<div class='table-scroll'><table class='file-table'>";
  html += "<tr><th>Type</th><th>Channel</th><th>File path</th><th>Status</th><th>Size</th><th>Action</th></tr>";

  uint16_t existingFileCount = 0;
  uint64_t listedBytes = 0;

  // Always show the eight expected channel calibration paths, even if the
  // filesystem is unavailable or a file has not yet been stored.
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    String path = channelConfigPath(ch);
    bool exists = littleFsReady && LittleFS.exists(path);
    uint64_t size = 0;
    if (exists) {
      File f = LittleFS.open(path, "r");
      if (f) {
        size = f.size();
        f.close();
      }
      existingFileCount++;
      listedBytes += size;
    }

    html += "<tr><td>Channel calibration</td><td>CH";
    html += String(ch + 1);
    html += "</td><td><code>";
    html += path;
    html += "</code></td><td>";
    if (!littleFsReady) html += "<span class='warn'>filesystem unavailable</span>";
    else html += exists ? "<span class='ok'>stored</span>" : "<span class='warn'>not stored</span>";
    html += "</td><td>";
    html += exists ? formatBytesHuman(size) : "-";
    html += "</td><td>";
    if (exists) appendLittleFsDeleteAction(html, path);
    else html += "-";
    html += "</td></tr>";
  }

  if (littleFsReady) {
    Dir dir = LittleFS.openDir("/");
    while (dir.next()) {
      String fullPath = dir.fileName();
      if (!fullPath.startsWith("/")) fullPath = "/" + fullPath;

      bool isExpectedChannelFile = false;
      for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
        if (fullPath == channelConfigPath(ch)) {
          isExpectedChannelFile = true;
          break;
        }
      }
      if (isExpectedChannelFile) continue;

      uint64_t size = dir.fileSize();
      existingFileCount++;
      listedBytes += size;

      html += "<tr><td>";
      html += littleFsFileType(fullPath);
      html += "</td><td>-</td><td><code>";
      html += fullPath;
      html += "</code></td><td><span class='ok'>stored</span></td><td>";
      html += formatBytesHuman(size);
      html += "</td><td>";
      if (fullPath == "/fw_update.bin.tmp") html += "<span class='warn'>protected</span>";
      else appendLittleFsDeleteAction(html, fullPath);
      html += "</td></tr>";
    }
  }

  html += "<tr><th colspan='3'>Existing files total</th><th>";
  html += String(existingFileCount);
  html += "</th><th>";
  html += formatBytesHuman(listedBytes);
  html += "</th><th>-</th></tr>";
  html += "</table></div>";
}

/**
 * @brief Append Little Fs Storage Info.
 * @param html HTML string that receives generated markup.
 */
void appendLittleFsStorageInfo(String& html) {
  html += "<h2>LittleFS storage status</h2>";

  html += "<p>LittleFS mount state: <code>";
  html += littleFsReady ? "ready" : "not ready - uploads will be RAM only until reboot";
  html += "</code></p>";

  if (!littleFsReady) {
    html += "<p class='warn'>No filesystem size or file list is available because LittleFS did not mount.</p>";
    appendLittleFsCombinedFiles(html);
    return;
  }

  FSInfo fsInfo;
  if (!LittleFS.info(fsInfo)) {
    html += "<p class='warn'>LittleFS.info() failed. Files may still be usable, but capacity information is unavailable.</p>";
    appendLittleFsCombinedFiles(html);
    return;
  }

  uint64_t totalBytes = fsInfo.totalBytes;
  uint64_t usedBytes = fsInfo.usedBytes;
  uint64_t freeBytes = 0;

  if (totalBytes >= usedBytes) {
    freeBytes = totalBytes - usedBytes;
  }

  double usedPercent = 0.0;
  if (totalBytes > 0) {
    usedPercent = (double(usedBytes) * 100.0) / double(totalBytes);
  }

  html += "<table>";
  html += "<tr><th>Metric</th><th>Value</th></tr>";

  html += "<tr><td>Total LittleFS size</td><td><code>";
  html += formatBytesHuman(totalBytes);
  html += "</code></td></tr>";

  html += "<tr><td>Used space</td><td><code>";
  html += formatBytesHuman(usedBytes);
  html += "</code></td></tr>";

  html += "<tr><td>Free space</td><td><code>";
  html += formatBytesHuman(freeBytes);
  html += "</code></td></tr>";

  html += "<tr><td>Used percent</td><td><code>";
  html += String(usedPercent, 2);
  html += "%</code></td></tr>";

  html += "<tr><td>Block size</td><td><code>";
  html += formatBytesHuman(fsInfo.blockSize);
  html += "</code></td></tr>";

  html += "<tr><td>Page size</td><td><code>";
  html += formatBytesHuman(fsInfo.pageSize);
  html += "</code></td></tr>";

  html += "<tr><td>Max open files</td><td><code>";
  html += String(fsInfo.maxOpenFiles);
  html += "</code></td></tr>";

  html += "<tr><td>Max path length</td><td><code>";
  html += String(fsInfo.maxPathLength);
  html += "</code></td></tr>";

  html += "</table>";

  appendLittleFsCombinedFiles(html);
}

