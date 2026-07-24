/**
 * @file http_diagnostics.cpp
 * @brief Atomic legacy diagnostics and state streaming.
 */
#include "http_api.h"

static void writeLegacyState(HttpResponseWriter& out, const RuntimeStateSnapshot& s) {
  out.printf("status=%s\n", s.status);
  out.printf("firmware_name=%s\nfirmware_version=%s\ntest_mode=%u\n", FIRMWARE_NAME, FIRMWARE_VERSION, s.testMode ? 1U : 0U);
  out.printf("firmware_build=%s %s\nfirmware_serial=%s-%s\n", FIRMWARE_BUILD_DATE, FIRMWARE_BUILD_TIME, FIRMWARE_VERSION, s.serial);
  out.printf("firmware_update_status=%s\nfirmware_update_in_progress=%u\n", s.firmwareUpdateStatus, firmwareUpdateInProgress ? 1U : 0U);
  out.printf("api_version=%s\nip=%s\nw5500_version=0x%02X\nws2812_gp=16\nscpi_port=5025\n",
             API_VERSION, s.ip, unsigned(s.w5500Version));
  out.printf("ethernet_fault=%u\nethernet_interface_started=%u\nethernet_services_started=%u\nethernet_link_up=%u\n",
             s.ethernetFault ? 1U : 0U, s.ethernetInterfaceStarted ? 1U : 0U,
             s.ethernetServicesStarted ? 1U : 0U, s.ethernetLinkUp ? 1U : 0U);
  out.printf("ethernet_recovery_state=%s\nethernet_recovery_attempts=%lu\nethernet_recovery_successes=%lu\nethernet_link_down_count=%lu\nethernet_last_transition_age_ms=%lu\n",
             ethernetRecoveryStateText(s.ethernetRecoveryState),
             static_cast<unsigned long>(s.ethernetRecoveryAttemptCount),
             static_cast<unsigned long>(s.ethernetRecoverySuccessCount),
             static_cast<unsigned long>(s.ethernetLinkDownCount),
             static_cast<unsigned long>(s.ethernetLastTransitionAgeMs));
  out.printf("littlefs=%s\ncalibration_saved_mask=%u\ncalibration_loaded_mask=%u\ncalibration_load_error_mask=%u\ncalibration_all_saved=%u\n",
             s.littleFsReady ? "ready" : "not_ready",
             unsigned(s.calibrationSavedMask), unsigned(s.calibrationLoadedMask),
             unsigned(s.calibrationLoadErrorMask), allChannelsHaveSavedCalibration() ? 1U : 0U);
  out.printf("shift_registers_ready=%u\noutputs_known_safe=%u\nfatal_safe_state=%u\ncpu_mhz=%lu\n",
             s.shiftRegistersReady ? 1U : 0U, s.outputsKnownSafe ? 1U : 0U,
             s.fatalSafeStateActive ? 1U : 0U, static_cast<unsigned long>(F_CPU / 1000000UL));
  out.printf("heap_total_bytes=%lu\nheap_used_bytes=%lu\nheap_free_bytes=%lu\nheap_used_percent=%.1f\n",
             static_cast<unsigned long>(s.heapTotalBytes), static_cast<unsigned long>(s.heapUsedBytes),
             static_cast<unsigned long>(s.heapFreeBytes), double(s.heapUsedPercent));
  out.printf("core0_load_percent=%.1f\nloop_rate_hz=%.1f\nloop_busy_max_us=%lu\nhttp_request_count=%lu\nscpi_command_count=%lu\n",
             double(s.core0LoadPercent), double(s.loopsPerSecond), static_cast<unsigned long>(s.runtimeLoopMaxUs),
             static_cast<unsigned long>(s.httpRequestCount), static_cast<unsigned long>(s.scpiCommandCount));
  out.printf("core1_state=%s\ncore1_startup_stage=%lu\ncore1_ready_token=%lu\ncore1_outputs_ready=%u\ncore1_fault=%u\n",
             s.core1EngineReady ? "ready" : "not_ready",
             static_cast<unsigned long>(s.core1StartupStage), static_cast<unsigned long>(s.core1ReadyToken),
             s.core1OutputsReady ? 1U : 0U, s.core1Fault ? 1U : 0U);
  out.printf("core1_heartbeat_ms=%lu\ncore1_loop_count=%lu\ncore1_command_count=%lu\ncore1_queue_overflow_count=%lu\n",
             static_cast<unsigned long>(s.core1HeartbeatMs), static_cast<unsigned long>(s.core1LoopCounter),
             static_cast<unsigned long>(s.core1CommandCounter), static_cast<unsigned long>(s.core1QueueOverflowCounter));
  out.printf("core1_loop_max_us=%lu\ncore1_stack_min_free_bytes=%lu\ncore1_event_count=%lu\ncore1_event_drop_count=%lu\n",
             static_cast<unsigned long>(s.core1LoopMaxUs), static_cast<unsigned long>(s.core1MinFreeStackBytes),
             static_cast<unsigned long>(s.core1EventCounter), static_cast<unsigned long>(s.core1EventDropCounter));
  if (s.outputSnapshotValid) {
    out.printf("core_snapshot_sequence=%lu\ncore_snapshot_last_command_sequence=%lu\ncore_snapshot_generation=%lu\ncore_snapshot_flags=%u\n",
               static_cast<unsigned long>(s.outputSnapshotSequence), static_cast<unsigned long>(s.outputLastCommandSequence),
               static_cast<unsigned long>(s.outputSafetyGeneration), unsigned(s.outputFlags));
  }
  const CoreTransportDiagnostics& t = s.transport;
  out.printf("core_transport_generation=%lu\ncore_transport_last_submitted_sequence=%lu\ncore_transport_last_completed_sequence=%lu\n",
             static_cast<unsigned long>(t.currentSafetyGeneration), static_cast<unsigned long>(t.lastSubmittedSequence),
             static_cast<unsigned long>(t.lastCompletedSequence));
  out.printf("core_transport_command_timeouts=%lu\ncore_transport_expired_rejects=%lu\ncore_transport_generation_rejects=%lu\n",
             static_cast<unsigned long>(t.commandTimeoutCount), static_cast<unsigned long>(t.commandExpiredCount),
             static_cast<unsigned long>(t.generationRejectCount));
  out.printf("core_transport_command_overflows=%lu\ncore_transport_result_overflows=%lu\ncore_transport_policy_installs=%lu\ncore_transport_core0_failsafe_count=%lu\n",
             static_cast<unsigned long>(t.commandQueueOverflowCount), static_cast<unsigned long>(t.resultQueueOverflowCount),
             static_cast<unsigned long>(t.policyInstallCount), static_cast<unsigned long>(t.core0FailsafeCount));
  out.printf("core_profile_transition_count=%lu\ncore_profile_failure_count=%lu\ncore_profile_break_before_make_count=%lu\n",
             static_cast<unsigned long>(t.profileTransitionCount), static_cast<unsigned long>(t.profileFailureCount),
             static_cast<unsigned long>(t.profileBreakBeforeMakeCount));
  out.printf("core_profile_last_duration_us=%lu\ncore_profile_max_duration_us=%lu\ncore_profile_last_clear_duration_us=%lu\ncore_profile_max_clear_duration_us=%lu\n",
             static_cast<unsigned long>(t.lastProfileDurationUs), static_cast<unsigned long>(t.maxProfileDurationUs),
             static_cast<unsigned long>(t.lastProfileClearDurationUs), static_cast<unsigned long>(t.maxProfileClearDurationUs));
  out.printf("target_search_last_candidates=%lu\ntarget_search_last_elapsed_us=%lu\ntarget_search_timeout_count=%lu\ntarget_search_cancel_count=%lu\n",
             static_cast<unsigned long>(s.targetCandidates), static_cast<unsigned long>(s.targetElapsedUs),
             static_cast<unsigned long>(s.targetTimeouts), static_cast<unsigned long>(s.targetCancels));
  out.write("configured_ip="); out.write(ipToString(DEVICE_IP)); out.write("\nconfigured_subnet="); out.write(ipToString(DEVICE_SUBNET));
  out.write("\nconfigured_gateway="); out.write(ipToString(DEVICE_GATEWAY)); out.write("\nconfigured_dns="); out.write(ipToString(DEVICE_DNS)); out.write("\n");
  out.printf("safety_min_ohm=%.3f\nsafety_max_ohm=%.3f\nsafety_max_active_bits=%u\n",
             safetyMinOhm[0], safetyMaxOhm[0], unsigned(safetyMaxActiveBits[0]));
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
    out.printf("safety_ch%u_min_ohm=%.3f\nsafety_ch%u_max_ohm=%.3f\nsafety_ch%u_max_active_bits=%u\n",
               unsigned(ch + 1), safetyMinOhm[ch], unsigned(ch + 1), safetyMaxOhm[ch],
               unsigned(ch + 1), unsigned(safetyMaxActiveBits[ch]));
  }
  out.printf("expert_mode=%u\nlast_scpi_command=%s\n", safetyExpertMode ? 1U : 0U, lastScpiCommand);
  out.printf("http_streamed_response_count=%lu\nhttp_calibration_page_last_temp_bytes=%lu\nhttp_calibration_page_peak_temp_bytes=%lu\n",
             static_cast<unsigned long>(s.streamedResponseCount), static_cast<unsigned long>(s.calibrationPageLastTempBytes),
             static_cast<unsigned long>(s.calibrationPagePeakTempBytes));
  out.printf("http_state_last_temp_bytes=%lu\nhttp_state_peak_temp_bytes=%lu\nhttp_method_rejected_count=%lu\nhttp_api_v1_request_count=%lu\n\n",
             static_cast<unsigned long>(s.stateLastTempBytes), static_cast<unsigned long>(s.statePeakTempBytes),
             static_cast<unsigned long>(s.methodRejectedCount), static_cast<unsigned long>(s.apiV1RequestCount));
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
    char resistance[32];
    formatOutputResistanceText(ch, s.masks[ch], resistance, sizeof(resistance));
    out.printf("ch%u=%04X resistance=%s count=%lu\n", unsigned(ch + 1), unsigned(s.masks[ch]),
               resistance, static_cast<unsigned long>(s.applyCounters[ch]));
  }
}

void handleState() {
  noteHttpRequest();
  const uint32_t heapBefore = getHeapFreeBytes();
  uint32_t minimumFree = heapBefore;
  RuntimeStateSnapshot snapshot{};
  captureRuntimeStateSnapshot(snapshot);
  const uint32_t afterSnapshot = getHeapFreeBytes();
  if (afterSnapshot < minimumFree) minimumFree = afterSnapshot;
  HttpResponseWriter out;
  if (!out.begin(200, "text/plain; charset=utf-8")) {
    server.send(500, "text/plain", "Unable to start state stream\n");
    return;
  }
  writeLegacyState(out, snapshot);
  out.end();
  const uint32_t heapAfter = getHeapFreeBytes();
  if (heapAfter < minimumFree) minimumFree = heapAfter;
  httpStateLastTempBytes = heapBefore >= minimumFree ? heapBefore - minimumFree : 0U;
  if (httpStateLastTempBytes > httpStatePeakTempBytes) httpStatePeakTempBytes = httpStateLastTempBytes;
}

