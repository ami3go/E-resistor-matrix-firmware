/**
 * @file scpi_server.cpp
 * @brief SCPI TCP command help, parser, command executor, and calibration-table query implementation.
 */

#include "scpi_api.h"
#include <math.h>


// ============================================================
// SCPI server
// ============================================================

/**
 * @brief Print the supported SCPI command list to an active TCP client.
 * @param client Connected TCP client used for the response.
 */


/**
 * @brief Parse SCPI channel-command aliases and return a zero-based channel index.
 * @param cmd Function parameter.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @param rest Function parameter.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool parseScpiChannelCommand(String cmd, uint8_t& channelIndex, String& rest) {
  char normalized[160];
  if (!normalizeScpiLine(cmd.c_str(), normalized, sizeof(normalized))) return false;
  const char* tail = nullptr;
  if (!parseScpiChannelCommandFixed(normalized, channelIndex, tail)) return false;
  rest = String(tail ? tail : "");
  rest.trim();
  return true;
}


// ============================================================
// Calibration-table SCPI output
//
// These queries let a PC driver download all calibrated branch values and do
// nearest-value / best-mask calculation on the PC side. The firmware-side
// nearest-mask function is intentionally kept for web UI/manual use.
//
// Output format is one line per query, LF terminated:
//   CH1:0,Q16,626.000000;1,Q15,1240.000000;...
//   CH1:...|CH2:...|...|CH8:...
//
// Fields per branch are:
//   bit_index,mosfet_name,resistance_ohm
// ============================================================

/**
 * @brief Print one channel calibration table in compact SCPI response format.
 * @param client Connected TCP client used for the response.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @return Result value; for bool, true means the operation succeeded.
 */
static void scpiPrintCalibrationChannelCompact(WiFiClient& client, uint8_t channelIndex) {
  client.print("CH");
  client.print(channelIndex + 1);
  client.print(":");

  for (uint8_t bit = 0; bit < BIT_COUNT; bit++) {
    if (bit > 0) client.print(";");
    client.print(bit);
    client.print(",");
    client.print(mosfetNameForBit(bit));
    client.print(",");
    const float resistanceOhm = getRuntimeResistanceOhms(channelIndex, bit);
    if (isfinite(resistanceOhm) && resistanceOhm > 0.0f) {
      client.print(String(double(resistanceOhm), 6));
    } else {
      client.print("NaN");
    }
  }
}

/**
 * @brief Print all channel calibration tables for PC-side nearest-mask calculation.
 * @param client Connected TCP client used for the response.
 * @return Result value; for bool, true means the operation succeeded.
 */
static void scpiPrintCalibrationAllCompact(WiFiClient& client) {
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    if (ch > 0) {
      client.print("|");
    }
    scpiPrintCalibrationChannelCompact(client, ch);
  }
  client.println();
}

/**
 * @brief Parse an optional channel selector after a calibration query command.
 * @param commandTail Command object to submit or process.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @return Result value; for bool, true means the operation succeeded.
 */
static bool parseOptionalCalibrationChannelArgument(const String& commandTail, uint8_t& channelIndex) {
  String arg = commandTail;
  arg.trim();

  if (arg.length() == 0) {
    return false;
  }

  arg.toUpperCase();
  while (arg.startsWith(":")) {
    arg.remove(0, 1);
    arg.trim();
  }

  if (arg.startsWith("CHANNEL")) {
    arg.remove(0, 7);
  } else if (arg.startsWith("CHAN")) {
    arg.remove(0, 4);
  } else if (arg.startsWith("CH")) {
    arg.remove(0, 2);
  }

  arg.trim();

  int ch = arg.toInt();
  if (ch < 1 || ch > int(CHANNEL_COUNT)) {
    return false;
  }

  channelIndex = uint8_t(ch - 1);
  return true;
}

/**
 * @brief Handle CAL:RES and calibration-table SCPI query aliases.
 * @param cmd Function parameter.
 * @param client Connected TCP client used for the response.
 * @return Result value; for bool, true means the operation succeeded.
 */
static bool tryHandleCalibrationQuery(String cmd, WiFiClient& client) {
  String upper = cmd;
  upper.toUpperCase();
  upper.trim();

  // Whole-table compact queries. Optional channel argument is accepted:
  //   CAL:RES?
  //   CAL:RES? 3
  //   CAL:RES? CH3
  const char* wholeQueryPrefixes[] = {
    "CAL:RES?",
    "CAL:RESISTANCE?",
    "CAL:RESISTORS?",
    "CAL:TABLE?",
    "CALIBRATION:RES?",
    "CALIBRATION:RESISTANCE?",
    "CALIBRATION:RESISTORS?",
    "CALIBRATION:TABLE?"
  };

  for (size_t i = 0; i < sizeof(wholeQueryPrefixes) / sizeof(wholeQueryPrefixes[0]); i++) {
    String prefix(wholeQueryPrefixes[i]);
    if (upper == prefix || upper.startsWith(prefix + " ") || upper.startsWith(prefix + ":")) {
      uint8_t ch = 0;
      String tail = cmd.substring(prefix.length());
      if (parseOptionalCalibrationChannelArgument(tail, ch)) {
        scpiPrintCalibrationChannelCompact(client, ch);
        client.println();
      } else {
        scpiPrintCalibrationAllCompact(client);
      }
      return true;
    }
  }

  // Per-channel query forms:
  //   CAL:CHAN1:RES?
  //   CAL:CHANNEL1:RESISTORS?
  //   CAL:CH1:TABLE?
  //   CALIBRATION:CHAN1:RES?
  String channelCmd = cmd;
  String channelUpper = upper;

  if (channelUpper.startsWith("CALIBRATION:")) {
    channelCmd.remove(0, 12);
    channelUpper.remove(0, 12);
  } else if (channelUpper.startsWith("CAL:")) {
    channelCmd.remove(0, 4);
    channelUpper.remove(0, 4);
  } else {
    return false;
  }

  uint8_t ch = 0;
  String rest;
  if (!parseScpiChannelCommand(channelCmd, ch, rest)) {
    return false;
  }

  String restUpper = rest;
  restUpper.toUpperCase();
  restUpper.trim();
  while (restUpper.startsWith(":")) {
    restUpper.remove(0, 1);
    restUpper.trim();
  }

  if (restUpper == "RES?" ||
      restUpper == "RESISTANCE?" ||
      restUpper == "RESISTORS?" ||
      restUpper == "TABLE?" ||
      restUpper == "VALUES?" ||
      restUpper == "CONF?" ||
      restUpper == "CONFIG?") {
    scpiPrintCalibrationChannelCompact(client, ch);
    client.println();
    return true;
  }

  return false;
}


/**
 * @brief Print a BEGIN/END-wrapped channel calibration CSV for SCPI file download.
 * @param client Connected TCP client used for the response.
 * @param channelIndex Zero-based channel index.
 */
static void scpiPrintCalibrationFileChannel(WiFiClient& client, uint8_t channelIndex) {
  bool exists = false;
  size_t sizeBytes = 0;
  getChannelConfigStorageInfo(channelIndex, exists, sizeBytes);

  client.print("#BEGIN CH");
  client.print(channelIndex + 1);
  client.print(" path=");
  client.print(channelConfigPath(channelIndex));
  client.print(" saved=");
  client.print(exists ? "1" : "0");
  client.print(" size=");
  client.println((uint32_t)sizeBytes);
  client.print(channelConfigToText(channelIndex));
  client.print("#END CH");
  client.println(channelIndex + 1);
}

/**
 * @brief Handle SCPI calibration/config file discovery and download queries.
 *
 * These commands are intended for the PC calibration GUI so it can recover the
 * active calibration tables stored or loaded on the device.
 *
 * Supported forms:
 *   CAL:FILES?
 *   CAL:FILE? CH1
 *   CAL:FILE? 1
 *   CAL:CHAN1:FILE?
 *   CAL:CHAN1:CONFIG?
 *   CAL:ALL:FILES?
 */
static bool tryHandleCalibrationFileQuery(String cmd, WiFiClient& client) {
  String upper = cmd;
  upper.toUpperCase();
  upper.trim();

  if (upper == "CAL:FILES?" || upper == "CAL:FILE:LIST?" ||
      upper == "CALIBRATION:FILES?" || upper == "CALIBRATION:FILE:LIST?") {
    client.println(calibrationFileListText());
    return true;
  }

  if (upper == "CAL:ALL:FILES?" || upper == "CAL:FILES:ALL?" ||
      upper == "CALIBRATION:ALL:FILES?" || upper == "CALIBRATION:FILES:ALL?") {
    client.print(allChannelConfigsToBundleText());
    client.println("#END ALL");
    return true;
  }

  const char* filePrefixes[] = {
    "CAL:FILE?",
    "CAL:CONFIG?",
    "CAL:CONF?",
    "CALIBRATION:FILE?",
    "CALIBRATION:CONFIG?",
    "CALIBRATION:CONF?"
  };

  for (size_t i = 0; i < sizeof(filePrefixes) / sizeof(filePrefixes[0]); i++) {
    String prefix(filePrefixes[i]);
    if (upper == prefix || upper.startsWith(prefix + " ") || upper.startsWith(prefix + ":")) {
      uint8_t ch = 0;
      String tail = cmd.substring(prefix.length());
      if (!parseOptionalCalibrationChannelArgument(tail, ch)) {
        client.println("ERR,-101,\"Missing or invalid channel for calibration file query\"");
        return true;
      }
      scpiPrintCalibrationFileChannel(client, ch);
      return true;
    }
  }

  // Per-channel file forms:
  //   CAL:CHAN1:FILE?
  //   CAL:CHAN1:CONF?
  //   CAL:CHANNEL1:CONFIG?
  String channelCmd = cmd;
  String channelUpper = upper;
  if (channelUpper.startsWith("CALIBRATION:")) {
    channelCmd.remove(0, 12);
    channelUpper.remove(0, 12);
  } else if (channelUpper.startsWith("CAL:")) {
    channelCmd.remove(0, 4);
    channelUpper.remove(0, 4);
  } else {
    return false;
  }

  uint8_t ch = 0;
  String rest;
  if (!parseScpiChannelCommand(channelCmd, ch, rest)) {
    return false;
  }

  String restUpper = rest;
  restUpper.toUpperCase();
  restUpper.trim();
  while (restUpper.startsWith(":")) {
    restUpper.remove(0, 1);
    restUpper.trim();
  }

  if (restUpper == "FILE?" || restUpper == "FILES?" ||
      restUpper == "CONF?" || restUpper == "CONFIG?" ||
      restUpper == "CSV?") {
    scpiPrintCalibrationFileChannel(client, ch);
    return true;
  }

  return false;
}

/**
 * @brief Parse and execute one received SCPI command line.
 * @param rawLine Function parameter.
 * @param client Connected TCP client used for the response.
 */
void processScpiLine(const char* rawLine, WiFiClient& client) {
  char normalized[160];
  if (!normalizeScpiLine(rawLine, normalized, sizeof(normalized))) {
    setLastError("-113,\"Invalid or oversized command\"");
    client.println("ERR,-113,\"Invalid or oversized command\"");
    return;
  }

  noteScpiCommand();
  strncpy(lastScpiCommand, normalized, sizeof(lastScpiCommand) - 1U);
  lastScpiCommand[sizeof(lastScpiCommand) - 1U] = '\0';
  Serial.print("SCPI: ");
  Serial.println(normalized);
  Serial.flush();

  RuntimeStateSnapshot atomicState{};
  const ScpiCommandId exactCommand = scpiLookupExactCommand(normalized);
  switch (exactCommand) {
    case ScpiCommandId::Help:
      scpiPrintHelp(client); return;
    case ScpiCommandId::IdnQuery:
      client.println(firmwareIdentityString()); return;
    case ScpiCommandId::SerialQuery:
      client.println(deviceSerialNumber); return;
    case ScpiCommandId::SystemVersionQuery:
    case ScpiCommandId::FirmwareVersionQuery:
      client.println(FIRMWARE_VERSION); return;
    case ScpiCommandId::FirmwareBuildQuery:
      client.print(FIRMWARE_BUILD_DATE); client.print(" "); client.println(FIRMWARE_BUILD_TIME); return;
    case ScpiCommandId::ClearStatus:
    case ScpiCommandId::ErrorClear:
      clearLastError(); client.println("OK"); return;
    case ScpiCommandId::ErrorQuery:
      client.println(lastError); clearLastError(); return;
    case ScpiCommandId::CalibrationStatusQuery:
      client.println(calibrationStorageStatusText()); return;
    case ScpiCommandId::UsbDiagnosticQuery:
      client.print("cdc_started=1,host_connected="); client.println(Serial ? "1" : "0"); return;
    case ScpiCommandId::SerialDiagnosticQuery: {
      char eventLine[160];
      snprintf(eventLine, sizeof(eventLine),
               "EVT ts_us=%lu core=0 seq=%lu level=1 code=SERIAL_TEST ch=0 mask=0x0000 detail=0 duration_us=0",
               static_cast<unsigned long>(micros()), static_cast<unsigned long>(scpiCommandCount));
      Serial.println(eventLine); Serial.flush(); appendLogEvent(eventLine); client.println("OK,SERIAL_TEST"); return;
    }
    case ScpiCommandId::CoreTransportQuery:
      captureRuntimeStateSnapshot(atomicState);
      client.print("startup_stage="); client.print(atomicState.core1StartupStage);
      client.print(",ready_token="); client.print(atomicState.core1ReadyToken);
      client.print(",generation="); client.print(atomicState.transport.currentSafetyGeneration);
      client.print(",last_submitted="); client.print(atomicState.transport.lastSubmittedSequence);
      client.print(",last_completed="); client.print(atomicState.transport.lastCompletedSequence);
      client.print(",command_overflows="); client.print(atomicState.transport.commandQueueOverflowCount);
      client.print(",result_overflows="); client.print(atomicState.transport.resultQueueOverflowCount);
      client.print(",timeouts="); client.print(atomicState.transport.commandTimeoutCount);
      client.print(",expired="); client.print(atomicState.transport.commandExpiredCount);
      client.print(",generation_rejects="); client.print(atomicState.transport.generationRejectCount);
      client.print(",invalid_commands="); client.print(atomicState.transport.invalidCommandCount);
      client.print(",policy_installs="); client.print(atomicState.transport.policyInstallCount);
      client.print(",core0_failsafe="); client.print(atomicState.transport.core0FailsafeCount);
      client.print(",profile_transitions="); client.print(atomicState.transport.profileTransitionCount);
      client.print(",profile_failures="); client.print(atomicState.transport.profileFailureCount);
      client.print(",profile_bbm_count="); client.print(atomicState.transport.profileBreakBeforeMakeCount);
      client.print(",profile_last_us="); client.print(atomicState.transport.lastProfileDurationUs);
      client.print(",profile_max_us="); client.print(atomicState.transport.maxProfileDurationUs);
      client.print(",profile_clear_last_us="); client.print(atomicState.transport.lastProfileClearDurationUs);
      client.print(",profile_clear_max_us="); client.println(atomicState.transport.maxProfileClearDurationUs); return;
    case ScpiCommandId::CoreProfileQuery:
      captureRuntimeStateSnapshot(atomicState);
      client.print("transitions="); client.print(atomicState.transport.profileTransitionCount);
      client.print(",failures="); client.print(atomicState.transport.profileFailureCount);
      client.print(",bbm_count="); client.print(atomicState.transport.profileBreakBeforeMakeCount);
      client.print(",last_us="); client.print(atomicState.transport.lastProfileDurationUs);
      client.print(",max_us="); client.print(atomicState.transport.maxProfileDurationUs);
      client.print(",clear_last_us="); client.print(atomicState.transport.lastProfileClearDurationUs);
      client.print(",clear_max_us="); client.println(atomicState.transport.maxProfileClearDurationUs); return;
    case ScpiCommandId::CoreSnapshotQuery:
      captureRuntimeStateSnapshot(atomicState);
      if (!atomicState.outputSnapshotValid) { client.println("ERR,snapshot_unavailable"); return; }
      client.print("snapshot_sequence="); client.print(atomicState.outputSnapshotSequence);
      client.print(",last_command_sequence="); client.print(atomicState.outputLastCommandSequence);
      client.print(",generation="); client.print(atomicState.outputSafetyGeneration);
      client.print(",flags="); client.print(atomicState.outputFlags);
      for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
        client.print(",ch"); client.print(ch + 1U); client.print("_mask=0x");
        if (atomicState.masks[ch] < 0x1000U) client.print('0');
        if (atomicState.masks[ch] < 0x0100U) client.print('0');
        if (atomicState.masks[ch] < 0x0010U) client.print('0');
        client.print(atomicState.masks[ch], HEX);
        client.print(",ch"); client.print(ch + 1U); client.print("_apply="); client.print(atomicState.applyCounters[ch]);
      }
      client.println(); return;
    case ScpiCommandId::StateQuery:
      captureRuntimeStateSnapshot(atomicState);
      for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
        if (ch) client.print(';');
        client.print("CH"); client.print(ch + 1U); client.print('=');
        if (atomicState.masks[ch] < 0x1000U) client.print('0');
        if (atomicState.masks[ch] < 0x0100U) client.print('0');
        if (atomicState.masks[ch] < 0x0010U) client.print('0');
        client.print(atomicState.masks[ch], HEX);
        char resistance[32];
        formatOutputResistanceText(ch, atomicState.masks[ch], resistance, sizeof(resistance));
        client.print(','); client.print(resistance);
      }
      client.println(); return;
    case ScpiCommandId::SystemStatusQuery:
      captureRuntimeStateSnapshot(atomicState);
      client.print("heap_free="); client.print(atomicState.heapFreeBytes);
      client.print(",core0_load="); client.print(atomicState.core0LoadPercent, 1);
      client.print(",http_count="); client.print(atomicState.httpRequestCount);
      client.print(",scpi_count="); client.print(atomicState.scpiCommandCount);
      client.print(",serial="); client.print(atomicState.serial);
      client.print(",fw_version="); client.print(FIRMWARE_VERSION);
      client.print(",cal_saved_mask="); client.print(atomicState.calibrationSavedMask);
      client.print(",cal_loaded_mask="); client.print(atomicState.calibrationLoadedMask);
      client.print(",cal_error_mask="); client.print(atomicState.calibrationLoadErrorMask);
      client.print(",min_ohm="); client.print(safetyMinOhm[0], 3);
      client.print(",max_ohm="); client.print(safetyMaxOhm[0], 3);
      for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
        client.print(",ch"); client.print(ch + 1U); client.print("_min_ohm="); client.print(safetyMinOhm[ch], 3);
        client.print(",ch"); client.print(ch + 1U); client.print("_max_ohm="); client.print(safetyMaxOhm[ch], 3);
        client.print(",ch"); client.print(ch + 1U); client.print("_max_bits="); client.print(safetyMaxActiveBits[ch]);
      }
      client.print(",expert="); client.print(safetyExpertMode ? "1" : "0");
      client.print(",target_candidates="); client.print(atomicState.targetCandidates);
      client.print(",target_elapsed_us="); client.print(atomicState.targetElapsedUs);
      client.print(",target_timeouts="); client.print(atomicState.targetTimeouts);
      client.print(",target_cancels="); client.print(atomicState.targetCancels);
      client.print(",core1_ready="); client.print(atomicState.core1EngineReady ? "1" : "0");
      client.print(",core1_cmds="); client.print(atomicState.core1CommandCounter);
      client.print(",core1_overflows="); client.print(atomicState.core1QueueOverflowCounter);
      client.print(",core1_loop_max_us="); client.print(atomicState.core1LoopMaxUs);
      client.print(",core1_stack_min_free="); client.print(atomicState.core1MinFreeStackBytes);
      client.print(",core1_events="); client.print(atomicState.core1EventCounter);
      client.print(",core1_event_drops="); client.print(atomicState.core1EventDropCounter);
      client.print(",transport_generation="); client.print(atomicState.transport.currentSafetyGeneration);
      client.print(",transport_timeouts="); client.print(atomicState.transport.commandTimeoutCount);
      client.print(",transport_expired="); client.print(atomicState.transport.commandExpiredCount);
      client.print(",transport_generation_rejects="); client.print(atomicState.transport.generationRejectCount);
      client.print(",transport_command_overflows="); client.print(atomicState.transport.commandQueueOverflowCount);
      client.print(",transport_result_overflows="); client.print(atomicState.transport.resultQueueOverflowCount);
      client.print(",transport_policy_installs="); client.print(atomicState.transport.policyInstallCount);
      client.print(",transport_core0_failsafe="); client.print(atomicState.transport.core0FailsafeCount);
      client.print(",profile_transitions="); client.print(atomicState.transport.profileTransitionCount);
      client.print(",profile_failures="); client.print(atomicState.transport.profileFailureCount);
      client.print(",profile_bbm_count="); client.print(atomicState.transport.profileBreakBeforeMakeCount);
      client.print(",profile_last_us="); client.print(atomicState.transport.lastProfileDurationUs);
      client.print(",profile_max_us="); client.print(atomicState.transport.maxProfileDurationUs);
      client.print(",profile_clear_last_us="); client.print(atomicState.transport.lastProfileClearDurationUs);
      client.print(",profile_clear_max_us="); client.print(atomicState.transport.maxProfileClearDurationUs);
      client.print(",http_streamed="); client.print(atomicState.streamedResponseCount);
      client.print(",http_cal_last_temp="); client.print(atomicState.calibrationPageLastTempBytes);
      client.print(",http_cal_peak_temp="); client.print(atomicState.calibrationPagePeakTempBytes);
      client.print(",http_state_last_temp="); client.print(atomicState.stateLastTempBytes);
      client.print(",http_state_peak_temp="); client.print(atomicState.statePeakTempBytes);
      client.print(",http_method_rejections="); client.print(atomicState.methodRejectedCount);
      client.print(",http_api_v1_requests="); client.println(atomicState.apiV1RequestCount); return;
    case ScpiCommandId::AllOff: {
      char reason[128] = {0};
      if (!forceAllOff(reason, sizeof(reason))) {
        client.print("ERR,-500,\""); client.print(reason[0] ? reason : "All-off failed"); client.println("\"");
      } else { clearLastError(); client.println("OK"); }
      return;
    }
    case ScpiCommandId::Unknown:
      break;
  }

  // Parameterized and calibration commands retain bounded String compatibility
  // at the protocol boundary. Exact commands above use no parser String.
  String cmd(normalized);
  String upper(normalized);

  if (tryHandleCalibrationFileQuery(cmd, client)) {
    return;
  }

  if (tryHandleCalibrationQuery(cmd, client)) {
    return;
  }

#ifdef ERESISTOR_TEST_MODE
  auto parseTestDurationMs = [&](uint32_t& durationMs) -> bool {
    String valueText = cmd.substring(cmd.lastIndexOf(' ') + 1);
    valueText.trim();
    if (valueText.length() == 0U || valueText.length() > 4U) return false;
    for (uint16_t i = 0; i < valueText.length(); ++i) {
      if (!isdigit(valueText[i])) return false;
    }
    const unsigned long parsed = strtoul(valueText.c_str(), nullptr, 10);
    if (parsed < 1UL || parsed > 5000UL) return false;
    durationMs = uint32_t(parsed);
    return true;
  };

  if (upper == "SYST:TEST:MODE?") {
    client.println("1");
    return;
  }
  if (upper.startsWith("SYST:TEST:CORE1:DELAY ")) {
    refreshCore0OutputMirror();
    for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
      if (channelMask[ch] != 0U) { client.println("ERR,outputs_must_be_off"); return; }
    }
    uint32_t delayMs = 0;
    if (!parseTestDurationMs(delayMs)) { client.println("ERR,duration_1_to_5000_ms"); return; }
    core1TestSetNextCommandDelayMs(delayMs);
    client.println("OK");
    return;
  }
  if (upper.startsWith("SYST:TEST:CORE1:PAUSE ")) {
    refreshCore0OutputMirror();
    for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
      if (channelMask[ch] != 0U) { client.println("ERR,outputs_must_be_off"); return; }
    }
    uint32_t pauseMs = 0;
    if (!parseTestDurationMs(pauseMs)) { client.println("ERR,duration_1_to_5000_ms"); return; }
    core1TestPauseProcessingMs(pauseMs);
    client.println("OK");
    return;
  }
  if (upper == "SYST:TEST:CORE1:INVALIDATE:NEXT") {
    refreshCore0OutputMirror();
    for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
      if (channelMask[ch] != 0U) { client.println("ERR,outputs_must_be_off"); return; }
    }
    core1TestInvalidateNextCommandGeneration();
    client.println("OK");
    return;
  }
  if (upper == "SYST:TEST:PROFILE:FAIL:NEXT") {
    refreshCore0OutputMirror();
    for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
      if (channelMask[ch] != 0U) { client.println("ERR,outputs_must_be_off"); return; }
    }
    core1TestFailNextProfileAfterClear();
    client.println("OK");
    return;
  }

  if (upper == "SYST:TEST:CORE1:INVALIDATE") {
    refreshCore0OutputMirror();
    for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
      if (channelMask[ch] != 0U) { client.println("ERR,outputs_must_be_off"); return; }
    }
    client.println(coreTransportInvalidateGeneration());
    return;
  }
#endif



  if (upper.startsWith("ROUT:ALL:MASK") || upper.startsWith("ROUTE:ALL:MASK")) {
    int maskPos = upper.indexOf("MASK");
    String values = cmd.substring(maskPos + 4);
    values.trim();
    if (values.startsWith("=")) {
      values.remove(0, 1);
      values.trim();
    }

    uint16_t masks[CHANNEL_COUNT];
    bool parseOk = true;
    int start = 0;
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
      int comma = values.indexOf(',', start);
      String token;
      if (i < CHANNEL_COUNT - 1) {
        if (comma < 0) {
          parseOk = false;
          break;
        }
        token = values.substring(start, comma);
        start = comma + 1;
      } else {
        token = values.substring(start);
      }
      token.trim();
      if (!parseHex16String(token, masks[i])) {
        parseOk = false;
        break;
      }
    }

    if (!parseOk) {
      setLastError("-128,\"Bad ROUT:ALL:MASK list\"");
      client.println("ERR,-128,\"Bad ROUT:ALL:MASK list\"");
      return;
    }

    char reason[128];
    if (!applyAllMasksSafely(masks, reason, sizeof(reason))) {
      setLastError(reason);
      client.print("ERR,-222,\"");
      client.print(reason);
      client.println("\"");
      return;
    }

    setStatus("All channel masks set by SCPI");
    appendLogEvent("All channel masks set by SCPI");
    clearLastError();
    client.println("OK");
    return;
  }

  if (upper == "ALL:OFF" || upper == ":ALL:OFF" || upper == "OUTP:ALL OFF" || upper == "OUTPUT:ALL OFF") {
    char reason[128] = {0};
    if (!forceAllOff(reason, sizeof(reason))) {
      client.print("ERR,-500,\"");
      client.print(reason[0] ? reason : "All-off failed");
      client.println("\"");
      return;
    }
    clearLastError();
    client.println("OK");
    return;
  }

  uint8_t ch = 0;
  String rest;

  if (!parseScpiChannelCommand(cmd, ch, rest)) {
    setLastError("-113,\"Undefined header\"");
    client.println("ERR,-113,\"Undefined header\"");
    return;
  }

  String restUpper = rest;
  restUpper.toUpperCase();

  while (restUpper.startsWith(":")) {
    restUpper.remove(0, 1);
    rest.remove(0, 1);
    rest.trim();
  }

  if (restUpper == "MASK?") {
    client.println(hex16(channelMask[ch]));
    return;
  }

  if (restUpper.startsWith("TARGET:CALC?")) {
    String valuePart = rest.substring(rest.indexOf('?') + 1);
    valuePart.trim();
    if (valuePart.startsWith("=")) {
      valuePart.remove(0, 1);
      valuePart.trim();
    }
    double requestedOhm = 0.0;
    if (!parseResistanceOhms(valuePart.c_str(), requestedOhm)) {
      setLastError("-128,\"Bad target resistance\"");
      client.println("ERR,-128,\"Bad target resistance\"");
      return;
    }

    TargetSearchResult result{};
    const uint32_t deadlineUs = safetyExpertMode ? 5000000UL : 2000000UL;
    if (!calculateNearestMaskForTarget(ch, requestedOhm, result, deadlineUs)) {
      if (result.timedOut) {
        setLastError("-365,\"Target search timeout\"");
        client.println("ERR,-365,\"Target search timeout\"");
      } else if (result.cancelled) {
        setLastError("-366,\"Target search cancelled\"");
        client.println("ERR,-366,\"Target search cancelled\"");
      } else {
        setLastError("-222,\"No safe target mask found\"");
        client.println("ERR,-222,\"No safe target mask found\"");
      }
      return;
    }

    client.print("requested_ohm="); client.print(String(result.requestedOhm, 6));
    client.print(",mask="); client.print(hex16(result.mask));
    client.print(",calculated_ohm="); client.print(String(result.calculatedOhm, 6));
    client.print(",absolute_error_ohm="); client.print(String(result.absoluteErrorOhm, 6));
    client.print(",error_percent="); client.print(String(result.errorPercent, 9));
    client.print(",candidates="); client.print(result.candidatesVisited);
    client.print(",elapsed_us="); client.println(result.elapsedUs);
    return;
  }

  if (restUpper.startsWith("MASK")) {
    String valuePart = rest.substring(4);
    valuePart.trim();

    if (valuePart.startsWith("=")) {
      valuePart.remove(0, 1);
      valuePart.trim();
    }

    uint16_t mask = 0;
    if (!parseHex16String(valuePart, mask)) {
      setLastError("-128,\"Numeric data not allowed\"");
      client.println("ERR,-128,\"Bad mask\"");
      return;
    }

    char safetyReason[128];
    if (!checkMaskSafety(ch, mask, safetyReason, sizeof(safetyReason))) {
      setLastError(safetyReason);
      client.print("ERR,-222,\"");
      client.print(safetyReason);
      client.println("\"");
      return;
    }

    if (!applyChannelMask(ch, mask)) {
      client.print("ERR,-500,\"");
      client.print(lastError[0] ? lastError : "Apply failed");
      client.println("\"");
      return;
    }

    char msg[96];
    snprintf(
      msg,
      sizeof(msg),
      "CH%u set to 0x%04X by SCPI",
      unsigned(ch + 1),
      unsigned(mask)
    );
    setStatus(msg);
    appendLogEvent(msg);
    clearLastError();

    client.println("OK");
    return;
  }

  if (restUpper == "RES?" || restUpper == "RESISTANCE?") {
    client.println(calculateOutputResistanceText(ch, channelMask[ch]));
    return;
  }

  if (restUpper == "CONF?" || restUpper == "CONFIG?") {
    client.print(channelConfigToText(ch));
    return;
  }

  setLastError("-113,\"Undefined header\"");
  client.println("ERR,-113,\"Undefined header\"");
}

/**
 * @brief Start the raw TCP SCPI server.
 */
void setupScpiServer() {
  scpiServer.begin();
  Serial.println("SCPI server started on TCP port 5025");
  Serial.flush();
}

/**
 * @brief Accept SCPI clients and process line-oriented commands.
 */
void handleScpiServer() {
  if (!scpiClient || !scpiClient.connected()) {
    WiFiClient newClient = scpiServer.accept();
    if (newClient) {
      scpiClient = newClient;
      scpiLineLen = 0;
      scpiDiscardUntilNewline = false;
      scpiClient.println("E-Resistor SCPI ready");
      Serial.println("SCPI client connected");
    }
    return;
  }

  while (scpiClient.available() > 0) {
    char c = char(scpiClient.read());

    if (scpiDiscardUntilNewline) {
      if (c == '\n') {
        scpiDiscardUntilNewline = false;
        scpiLineLen = 0;
      }
      continue;
    }

    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      scpiLine[scpiLineLen] = '\0';
      processScpiLine(scpiLine, scpiClient);
      scpiLineLen = 0;
      continue;
    }

    if (scpiLineLen < sizeof(scpiLine) - 1) {
      scpiLine[scpiLineLen++] = c;
    } else {
      scpiLineLen = 0;
      scpiDiscardUntilNewline = true;
      setLastError("-350,\"Input buffer overflow\"");
      scpiClient.println("ERR,-350,\"Input buffer overflow\"");
    }
  }
}

