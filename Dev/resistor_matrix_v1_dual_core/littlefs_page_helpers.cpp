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
 * @brief Append Little Fs Channel Config Status.
 * @param html HTML string that receives generated markup.
 */
void appendLittleFsChannelConfigStatus(String& html) {
  html += "<h3>Expected channel config files</h3>";
  html += "<table>";
  html += "<tr><th>Channel</th><th>File path</th><th>Status</th><th>Size</th></tr>";

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
    }

    html += "<tr><td>CH";
    html += String(ch + 1);
    html += "</td><td><code>";
    html += path;
    html += "</code></td><td>";
    html += exists ? "<span class='ok'>stored</span>" : "<span class='warn'>not stored</span>";
    html += "</td><td>";
    html += exists ? formatBytesHuman(size) : "-";
    html += "</td></tr>";
  }

  html += "</table>";
}

/**
 * @brief Append Little Fs Stored Files.
 * @param html HTML string that receives generated markup.
 */
void appendLittleFsStoredFiles(String& html) {
  html += "<h3>Stored files in LittleFS root</h3>";

  if (!littleFsReady) {
    html += "<p class='warn'>LittleFS is not ready, so stored files cannot be listed.</p>";
    return;
  }

  Dir dir = LittleFS.openDir("/");
  uint16_t fileCount = 0;
  uint64_t listedBytes = 0;

  html += "<table>";
  html += "<tr><th>#</th><th>File name</th><th>Full path</th><th>Size</th></tr>";

  while (dir.next()) {
    String name = dir.fileName();
    String fullPath = name;

    if (!fullPath.startsWith("/")) {
      fullPath = "/" + fullPath;
    }

    uint64_t size = dir.fileSize();
    listedBytes += size;
    fileCount++;

    html += "<tr><td>";
    html += String(fileCount);
    html += "</td><td><code>";
    html += name;
    html += "</code></td><td><code>";
    html += fullPath;
    html += "</code></td><td>";
    html += formatBytesHuman(size);
    html += "</td></tr>";
  }

  if (fileCount == 0) {
    html += "<tr><td colspan='4'><span class='warn'>No files stored in root directory.</span></td></tr>";
  } else {
    html += "<tr><th colspan='3'>Listed files total</th><th>";
    html += formatBytesHuman(listedBytes);
    html += "</th></tr>";
  }

  html += "</table>";
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
    appendLittleFsChannelConfigStatus(html);
    return;
  }

  FSInfo fsInfo;
  if (!LittleFS.info(fsInfo)) {
    html += "<p class='warn'>LittleFS.info() failed. Files may still be usable, but capacity information is unavailable.</p>";
    appendLittleFsChannelConfigStatus(html);
    appendLittleFsStoredFiles(html);
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

  appendLittleFsChannelConfigStatus(html);
  appendLittleFsStoredFiles(html);
}

