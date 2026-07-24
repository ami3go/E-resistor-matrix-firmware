/**
 * @file http_api_v1.cpp
 * @brief Canonical versioned Gate 5 JSON API.
 */
#include "http_api.h"

static void writeJsonEscaped(HttpResponseWriter& out, const char* text) {
  out.writeChar('"');
  if (text) {
    for (const char* p = text; *p; ++p) {
      switch (*p) {
        case '"': out.write("\\\""); break;
        case '\\': out.write("\\\\"); break;
        case '\n': out.write("\\n"); break;
        case '\r': out.write("\\r"); break;
        case '\t': out.write("\\t"); break;
        default:
          if (static_cast<unsigned char>(*p) >= 0x20U) out.writeChar(*p);
          break;
      }
    }
  }
  out.writeChar('"');
}

static void noteApiV1Request() {
  noteHttpRequest();
  ++httpApiV1RequestCount;
}

void handleApiV1Health() {
  noteApiV1Request();
  RuntimeStateSnapshot s{}; captureRuntimeStateSnapshot(s);
  HttpResponseWriter out; if (!out.begin(200, "application/json")) return;
  out.write("{\"schema_version\":1,\"api_version\":\"v1\",\"status\":"); writeJsonEscaped(out, s.status);
  out.printf(",\"ready\":%s,\"outputs_safe\":%s,\"firmware_version\":\"%s\",\"serial\":",
             (s.core1EngineReady && !s.fatalSafeStateActive) ? "true" : "false",
             s.outputsKnownSafe ? "true" : "false", FIRMWARE_VERSION);
  writeJsonEscaped(out, s.serial); out.write("}\n"); out.end();
}

void handleApiV1State() {
  noteApiV1Request();
  RuntimeStateSnapshot s{}; captureRuntimeStateSnapshot(s);
  HttpResponseWriter out; if (!out.begin(200, "application/json")) return;
  out.write("{\"schema_version\":1,\"api_version\":\"v1\",\"identity\":{");
  out.printf("\"vendor\":\"%s\",\"name\":\"%s\",\"version\":\"%s\",\"serial\":", FIRMWARE_VENDOR, FIRMWARE_NAME, FIRMWARE_VERSION);
  writeJsonEscaped(out, s.serial);
  out.write("},\"runtime\":{");
  out.printf("\"status\":"); writeJsonEscaped(out, s.status);
  out.printf(",\"test_mode\":%s,\"uptime_ms\":%lu,\"heap_free_bytes\":%lu,\"core0_load_percent\":%.1f",
             s.testMode ? "true" : "false", static_cast<unsigned long>(s.uptimeMs),
             static_cast<unsigned long>(s.heapFreeBytes), double(s.core0LoadPercent));
  out.write("},\"safety\":{");
  out.printf("\"outputs_known_safe\":%s,\"fatal_safe_state\":%s,\"core1_ready\":%s,\"core1_fault\":%s",
             s.outputsKnownSafe ? "true" : "false", s.fatalSafeStateActive ? "true" : "false",
             s.core1EngineReady ? "true" : "false", s.core1Fault ? "true" : "false");
  out.write("},\"calibration\":{");
  out.printf("\"saved_mask\":%u,\"loaded_mask\":%u,\"error_mask\":%u",
             unsigned(s.calibrationSavedMask), unsigned(s.calibrationLoadedMask), unsigned(s.calibrationLoadErrorMask));
  out.write("},\"channels\":[");
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
    if (ch) out.writeChar(',');
    char resistance[32];
    formatOutputResistanceText(ch, s.masks[ch], resistance, sizeof(resistance));
    out.printf("{\"channel\":%u,\"mask\":\"%04X\",\"resistance\":", unsigned(ch + 1), unsigned(s.masks[ch]));
    writeJsonEscaped(out, resistance);
    out.printf(",\"apply_count\":%lu}", static_cast<unsigned long>(s.applyCounters[ch]));
  }
  out.write("]}\n"); out.end();
}

void handleApiV1Channels() {
  noteApiV1Request();
  RuntimeStateSnapshot s{}; captureRuntimeStateSnapshot(s);
  HttpResponseWriter out; if (!out.begin(200, "application/json")) return;
  out.write("{\"schema_version\":1,\"channels\":[");
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
    if (ch) out.writeChar(',');
    double ohms = 0.0; const bool finiteValue = calculateEquivalentOhms(ch, s.masks[ch], ohms);
    out.printf("{\"channel\":%u,\"mask\":\"%04X\",\"active_bits\":%u,\"apply_count\":%lu,\"resistance_ohm\":",
               unsigned(ch + 1), unsigned(s.masks[ch]), unsigned(countActiveBits16(s.masks[ch])),
               static_cast<unsigned long>(s.applyCounters[ch]));
    if (finiteValue) out.printf("%.6f", ohms); else out.write("null");
    out.writeChar('}');
  }
  out.write("]}\n"); out.end();
}

void handleApiV1Diagnostics() {
  noteApiV1Request();
  RuntimeStateSnapshot s{}; captureRuntimeStateSnapshot(s);
  const CoreTransportDiagnostics& t = s.transport;
  HttpResponseWriter out; if (!out.begin(200, "application/json")) return;
  out.printf("{\"schema_version\":1,\"startup\":{\"stage\":%lu,\"ready_token\":%lu},",
             static_cast<unsigned long>(s.core1StartupStage), static_cast<unsigned long>(s.core1ReadyToken));
  out.printf("\"transport\":{\"timeouts\":%lu,\"expired_rejects\":%lu,\"generation_rejects\":%lu,\"command_overflows\":%lu,\"result_overflows\":%lu,\"event_drops\":%lu},",
             static_cast<unsigned long>(t.commandTimeoutCount), static_cast<unsigned long>(t.commandExpiredCount),
             static_cast<unsigned long>(t.generationRejectCount), static_cast<unsigned long>(t.commandQueueOverflowCount),
             static_cast<unsigned long>(t.resultQueueOverflowCount), static_cast<unsigned long>(s.core1EventDropCounter));
  out.printf("\"http\":{\"streamed_responses\":%lu,\"calibration_page_last_temp_bytes\":%lu,\"calibration_page_peak_temp_bytes\":%lu,\"gate4_calibration_baseline_bytes\":%lu,\"state_last_temp_bytes\":%lu,\"state_peak_temp_bytes\":%lu,\"method_rejections\":%lu,\"api_v1_requests\":%lu},",
             static_cast<unsigned long>(s.streamedResponseCount), static_cast<unsigned long>(s.calibrationPageLastTempBytes),
             static_cast<unsigned long>(s.calibrationPagePeakTempBytes), static_cast<unsigned long>(GATE4_CALIBRATION_TEMP_HEAP_BASELINE_BYTES),
             static_cast<unsigned long>(s.stateLastTempBytes), static_cast<unsigned long>(s.statePeakTempBytes),
             static_cast<unsigned long>(s.methodRejectedCount), static_cast<unsigned long>(s.apiV1RequestCount));
  out.printf("\"heap\":{\"total_bytes\":%lu,\"used_bytes\":%lu,\"free_bytes\":%lu}}\n",
             static_cast<unsigned long>(s.heapTotalBytes), static_cast<unsigned long>(s.heapUsedBytes), static_cast<unsigned long>(s.heapFreeBytes));
  out.end();
}

void handleApiV1CalibrationFiles() { noteApiV1Request(); handleCalibrationFilesApi(); }
void handleApiV1CalibrationDownload() { noteApiV1Request(); handleDownloadConfig(); }
void handleApiV1CalibrationDownloadAll() { noteApiV1Request(); handleCalibrationDownloadAll(); }
