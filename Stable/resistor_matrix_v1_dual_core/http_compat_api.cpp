/**
 * @file http_compat_api.cpp
 * @brief Small compatibility endpoints and streamed calibration downloads.
 */
#include "http_api.h"

static void streamChannelCalibration(HttpResponseWriter& out, uint8_t channelIndex) {
  out.write("bit,mosfet_name,nominal_resistance\n");
  for (uint8_t bit = 0; bit < BIT_COUNT; ++bit) {
    const float resistance = getRuntimeResistanceOhms(channelIndex, bit);
    if (isfinite(resistance) && resistance > 0.0f) {
      out.printf("%u,%s,%.6f\n", unsigned(bit), mosfetNameForBit(bit), double(resistance));
    } else {
      out.printf("%u,%s,NaN\n", unsigned(bit), mosfetNameForBit(bit));
    }
  }
}

static void streamCalibrationBundle(bool attachment) {
  if (attachment) {
    String filename = "e-resistor-calibration-";
    filename += deviceSerialNumber;
    filename += ".txt";
    server.sendHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
    server.sendHeader("X-Content-Type-Options", "nosniff");
  }
  HttpResponseWriter out;
  if (!out.begin(200, "text/plain; charset=utf-8")) return;
  out.write("# E-Resistor calibration bundle\n# format=1\n# firmware=");
  out.write(FIRMWARE_VERSION);
  out.write("\n# serial=");
  out.write(deviceSerialNumber);
  out.write("\n");
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
    bool exists = false;
    size_t sizeBytes = 0;
    getChannelConfigStorageInfo(ch, exists, sizeBytes);
    const String path = channelConfigPath(ch);
    out.printf("#BEGIN CH%u path=%s saved=%u size=%lu\n",
               unsigned(ch + 1), path.c_str(), exists ? 1U : 0U,
               static_cast<unsigned long>(sizeBytes));
    streamChannelCalibration(out, ch);
    out.printf("#END CH%u\n", unsigned(ch + 1));
    out.flush();
  }
  out.end();
}

void handlePing() {
  noteHttpRequest();
  sendNoCacheHeaders();
  server.send(200, "text/plain", "pong\n");
}

void handleDownloadConfig() {
  noteHttpRequest();
  uint8_t ch = 0;
  if (!parseChannel(ch)) {
    server.send(400, "text/plain", "Bad channel. Use ch=1..8\n");
    return;
  }
  HttpResponseWriter out;
  if (!out.begin(200, "text/plain; charset=utf-8")) return;
  streamChannelCalibration(out, ch);
  out.end();
}

void handleCalibrationFilesApi() {
  noteHttpRequest();
  sendNoCacheHeaders();
  server.send(200, "text/plain", calibrationFileListText() + "\n");
}

void handleCalibrationDownloadAll() {
  noteHttpRequest();
  streamCalibrationBundle(false);
}

void handleCalibrationDownloadAllFile() {
  noteHttpRequest();
  streamCalibrationBundle(true);
}
