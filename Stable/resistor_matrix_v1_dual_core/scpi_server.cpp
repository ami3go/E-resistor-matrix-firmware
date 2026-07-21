/**
 * @file scpi_server.cpp
 * @brief SCPI TCP command help, parser, command executor, and calibration-table query implementation.
 */

#include "app.h"
#include <math.h>


// ============================================================
// SCPI server
// ============================================================

/**
 * @brief Print the supported SCPI command list to an active TCP client.
 * @param client Connected TCP client used for the response.
 */
void scpiPrintHelp(WiFiClient& client) {
  client.println("Commands:");
  client.println("*IDN?");
  client.println("SYST:SER?");
  client.println("SYST:VERS?");
  client.println("FIRM:VERS?");
  client.println("FIRM:BUILD?");
  client.println("*CLS");
  client.println("SYST:ERR?");
  client.println("SYST:ERR:CLEAR");
  client.println("STATE?");
  client.println("SYST:STAT?");
  client.println("SYST:CORE:TRANSPORT?           - numeric command transport diagnostics");
  client.println("SYST:CORE:SNAPSHOT?            - coherent Core 1 output snapshot");
  client.println("SYST:CORE:PROFILE?             - Gate 4 profile transition diagnostics");
#ifdef ERESISTOR_TEST_MODE
  client.println("SYST:TEST:MODE?                - returns 1 for fault-injection build");
  client.println("SYST:TEST:CORE1:DELAY <ms>     - delay next Core 1 command; outputs must be OFF");
  client.println("SYST:TEST:CORE1:PAUSE <ms>     - pause command dequeue; outputs must be OFF");
  client.println("SYST:TEST:CORE1:INVALIDATE:NEXT - invalidate the next queued command; outputs must be OFF");
  client.println("SYST:TEST:CORE1:INVALIDATE      - immediately invalidate generation; outputs must be OFF");
  client.println("SYST:TEST:PROFILE:FAIL:NEXT     - fail next profile after global clear phase");
#endif
  client.println("CAL:RES? or CAL:RESISTORS?          - all calibration branch values");
  client.println("CAL:CHANnel<n>:RES?                 - one channel calibration branch values");
  client.println("CAL:FILES?                          - list saved calibration/config files");
  client.println("CAL:FILE? CH<n>                     - download one channel calibration CSV");
  client.println("CAL:ALL:FILES?                      - download all channel calibration CSV tables");
  client.println("ALL:OFF");
  client.println("OUTP:ALL OFF");
  client.println("ROUT:ALL:MASK <m1>,<m2>,...,<m8>");
  client.println("CH<n>:MASK? or ROUT:CHANnel<n>:MASK?");
  client.println("CH<n>:MASK <hex> or ROUT:CHANnel<n>:MASK <hex>");
  client.println("CH<n>:RES?");
  client.println("CH<n>:TARGET:CALC? <ohm>       - read-only nearest-mask calculation");
  client.println("SYST:DIAG:SERIAL?              - emit a USB serial test event");
  client.println("SYST:DIAG:USB?                 - report USB CDC start/host state");
  client.println("CAL:STATUS?                    - report saved/loaded calibration masks");
  client.println("CH<n>:CONF?");
  client.println("Example: CH1:MASK 0001");
}

/**
 * @brief Parse SCPI channel-command aliases and return a zero-based channel index.
 * @param cmd Function parameter.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @param rest Function parameter.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool parseScpiChannelCommand(String cmd, uint8_t& channelIndex, String& rest) {
  cmd.trim();

  while (cmd.startsWith(":")) {
    cmd.remove(0, 1);
    cmd.trim();
  }

  String upper = cmd;
  upper.toUpperCase();

  // Accept both the original prototype format and the specification-style
  // ROUT:CHANnel<n>:... / ROUT:CHAN<n>:... forms.
  if (upper.startsWith("ROUTE:")) {
    cmd.remove(0, 6);
    upper.remove(0, 6);
  } else if (upper.startsWith("ROUT:")) {
    cmd.remove(0, 5);
    upper.remove(0, 5);
  }

  int pos = -1;
  if (upper.startsWith("CHANNEL")) {
    pos = 7;
  } else if (upper.startsWith("CHAN")) {
    pos = 4;
  } else if (upper.startsWith("CH")) {
    pos = 2;
  } else {
    return false;
  }

  if (pos >= int(upper.length()) || !isdigit(upper[pos])) {
    return false;
  }

  int ch = 0;
  while (pos < int(upper.length()) && isdigit(upper[pos])) {
    ch = ch * 10 + (upper[pos] - '0');
    pos++;
  }

  if (ch < 1 || ch > int(CHANNEL_COUNT)) {
    return false;
  }

  channelIndex = uint8_t(ch - 1);
  rest = cmd.substring(pos);
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
  refreshCore0OutputMirror();
  String cmd(rawLine);
  cmd.trim();

  if (cmd.length() == 0) {
    return;
  }

  noteScpiCommand();
  strncpy(lastScpiCommand, cmd.c_str(), sizeof(lastScpiCommand) - 1);
  lastScpiCommand[sizeof(lastScpiCommand) - 1] = '\0';

  Serial.print("SCPI: ");
  Serial.println(cmd);
  Serial.flush();

  String upper = cmd;
  upper.toUpperCase();

  if (upper == "*IDN?") {
    client.println(firmwareIdentityString());
    return;
  }

  if (upper == "SYST:SER?" || upper == "SYSTEM:SERIAL?" || upper == "SERIAL?" || upper == "SER?") {
    client.println(deviceSerialNumber);
    return;
  }

  if (upper == "SYST:VERS?" || upper == "SYSTEM:VERSION?" || upper == "FIRM:VERS?" || upper == "FIRMWARE:VERSION?" || upper == "VERS?") {
    client.println(FIRMWARE_VERSION);
    return;
  }

  if (upper == "FIRM:BUILD?" || upper == "FIRMWARE:BUILD?") {
    client.print(FIRMWARE_BUILD_DATE);
    client.print(" ");
    client.println(FIRMWARE_BUILD_TIME);
    return;
  }

  if (upper == "CAL:STATUS?" || upper == "CALIBRATION:STATUS?") {
    client.println(calibrationStorageStatusText());
    return;
  }

  if (upper == "SYST:DIAG:USB?" || upper == "SYSTEM:DIAGNOSTIC:USB?") {
    client.print("cdc_started=1,host_connected=");
    client.println(Serial ? "1" : "0");
    return;
  }

  if (upper == "SYST:DIAG:SERIAL?" || upper == "SYSTEM:DIAGNOSTIC:SERIAL?") {
    char eventLine[160];
    snprintf(
      eventLine,
      sizeof(eventLine),
      "EVT ts_us=%lu core=0 seq=%lu level=1 code=SERIAL_TEST ch=0 mask=0x0000 detail=0 duration_us=0",
      static_cast<unsigned long>(micros()),
      static_cast<unsigned long>(scpiCommandCount)
    );
    Serial.println(eventLine);
    Serial.flush();
    // Preserve the diagnostic in the firmware event log even when the host has
    // not asserted USB CDC DTR yet. The COM-port HIL still verifies the USB copy.
    appendLogEvent(eventLine);
    client.println("OK,SERIAL_TEST");
    return;
  }

  if (upper == "*CLS" || upper == "SYST:ERR:CLEAR" || upper == "SYSTEM:ERROR:CLEAR") {
    clearLastError();
    client.println("OK");
    return;
  }

  if (upper == "SYST:ERR?" || upper == "SYSTEM:ERROR?") {
    client.println(lastError);
    clearLastError();
    return;
  }

  if (upper == "HELP?" || upper == "HELP") {
    scpiPrintHelp(client);
    return;
  }

  if (tryHandleCalibrationFileQuery(cmd, client)) {
    return;
  }

  if (tryHandleCalibrationQuery(cmd, client)) {
    return;
  }

  if (upper == "SYST:CORE:TRANSPORT?" || upper == "SYSTEM:CORE:TRANSPORT?") {
    CoreTransportDiagnostics d{};
    getCoreTransportDiagnostics(d);
    client.print("generation="); client.print(d.currentSafetyGeneration);
    client.print(",last_submitted="); client.print(d.lastSubmittedSequence);
    client.print(",last_completed="); client.print(d.lastCompletedSequence);
    client.print(",command_overflows="); client.print(d.commandQueueOverflowCount);
    client.print(",result_overflows="); client.print(d.resultQueueOverflowCount);
    client.print(",timeouts="); client.print(d.commandTimeoutCount);
    client.print(",expired="); client.print(d.commandExpiredCount);
    client.print(",generation_rejects="); client.print(d.generationRejectCount);
    client.print(",invalid_commands="); client.print(d.invalidCommandCount);
    client.print(",policy_installs="); client.print(d.policyInstallCount);
    client.print(",core0_failsafe="); client.print(d.core0FailsafeCount);
    client.print(",profile_transitions="); client.print(d.profileTransitionCount);
    client.print(",profile_failures="); client.print(d.profileFailureCount);
    client.print(",profile_bbm_count="); client.print(d.profileBreakBeforeMakeCount);
    client.print(",profile_last_us="); client.print(d.lastProfileDurationUs);
    client.print(",profile_max_us="); client.print(d.maxProfileDurationUs);
    client.print(",profile_clear_last_us="); client.print(d.lastProfileClearDurationUs);
    client.print(",profile_clear_max_us="); client.println(d.maxProfileClearDurationUs);
    return;
  }

  if (upper == "SYST:CORE:PROFILE?" || upper == "SYSTEM:CORE:PROFILE?") {
    CoreTransportDiagnostics d{};
    getCoreTransportDiagnostics(d);
    client.print("transitions="); client.print(d.profileTransitionCount);
    client.print(",failures="); client.print(d.profileFailureCount);
    client.print(",bbm_count="); client.print(d.profileBreakBeforeMakeCount);
    client.print(",last_us="); client.print(d.lastProfileDurationUs);
    client.print(",max_us="); client.print(d.maxProfileDurationUs);
    client.print(",clear_last_us="); client.print(d.lastProfileClearDurationUs);
    client.print(",clear_max_us="); client.println(d.maxProfileClearDurationUs);
    return;
  }

  if (upper == "SYST:CORE:SNAPSHOT?" || upper == "SYSTEM:CORE:SNAPSHOT?") {
    CoreOutputSnapshot snapshot{};
    if (!readCoreOutputSnapshot(snapshot)) {
      client.println("ERR,snapshot_unavailable");
      return;
    }
    client.print("snapshot_sequence="); client.print(snapshot.snapshotSequence);
    client.print(",last_command_sequence="); client.print(snapshot.lastCommandSequence);
    client.print(",generation="); client.print(snapshot.safetyGeneration);
    client.print(",flags="); client.print(snapshot.flags);
    for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
      client.print(",ch"); client.print(ch + 1U); client.print("_mask=0x");
      if (snapshot.masks[ch] < 0x1000U) client.print('0');
      if (snapshot.masks[ch] < 0x0100U) client.print('0');
      if (snapshot.masks[ch] < 0x0010U) client.print('0');
      client.print(snapshot.masks[ch], HEX);
      client.print(",ch"); client.print(ch + 1U); client.print("_apply=");
      client.print(snapshot.applyCounter[ch]);
    }
    client.println();
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

  if (upper == "SYST:STAT?" || upper == "SYSTEM:STATUS?") {
    client.print("heap_free=");
    client.print(getHeapFreeBytes());
    client.print(",core0_load=");
    client.print(String(runtimeCore0LoadPct, 1));
    client.print(",http_count=");
    client.print(httpRequestCount);
    client.print(",scpi_count=");
    client.print(scpiCommandCount);
    client.print(",serial=");
    client.print(deviceSerialNumber);
    client.print(",fw_version=");
    client.print(FIRMWARE_VERSION);
    client.print(",cal_saved_mask=");
    client.print(calibrationSavedMask);
    client.print(",cal_loaded_mask=");
    client.print(calibrationLoadedMask);
    client.print(",cal_error_mask=");
    client.print(calibrationLoadErrorMask);
    // Legacy summary fields report CH1; channel-specific values follow.
    client.print(",min_ohm=");
    client.print(String(safetyMinOhm[0], 3));
    client.print(",max_ohm=");
    client.print(String(safetyMaxOhm[0], 3));
    for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
      client.print(",ch"); client.print(ch + 1); client.print("_min_ohm=");
      client.print(String(safetyMinOhm[ch], 3));
      client.print(",ch"); client.print(ch + 1); client.print("_max_ohm=");
      client.print(String(safetyMaxOhm[ch], 3));
      client.print(",ch"); client.print(ch + 1); client.print("_max_bits=");
      client.print(safetyMaxActiveBits[ch]);
    }
    client.print(",expert=");
    client.print(safetyExpertMode ? "1" : "0");
    client.print(",target_candidates=");
    client.print(targetSearchLastCandidates);
    client.print(",target_elapsed_us=");
    client.print(targetSearchLastElapsedUs);
    client.print(",target_timeouts=");
    client.print(targetSearchTimeoutCount);
    client.print(",target_cancels=");
    client.print(targetSearchCancelCount);
    client.print(",core1_ready=");
    client.print(core1EngineReady ? "1" : "0");
    client.print(",core1_cmds=");
    client.print((uint32_t)core1CommandCounter);
    client.print(",core1_overflows=");
    client.print((uint32_t)core1QueueOverflowCounter);
    client.print(",core1_loop_max_us=");
    client.print((uint32_t)core1LoopMaxUs);
    client.print(",core1_stack_min_free=");
    client.print((uint32_t)core1MinFreeStackBytes);
    client.print(",core1_events=");
    client.print((uint32_t)core1EventCounter);
    client.print(",core1_event_drops=");
    client.print((uint32_t)core1EventDropCounter);
    CoreTransportDiagnostics transport{};
    getCoreTransportDiagnostics(transport);
    client.print(",transport_generation="); client.print(transport.currentSafetyGeneration);
    client.print(",transport_timeouts="); client.print(transport.commandTimeoutCount);
    client.print(",transport_expired="); client.print(transport.commandExpiredCount);
    client.print(",transport_generation_rejects="); client.print(transport.generationRejectCount);
    client.print(",transport_command_overflows="); client.print(transport.commandQueueOverflowCount);
    client.print(",transport_result_overflows="); client.print(transport.resultQueueOverflowCount);
    client.print(",transport_policy_installs="); client.print(transport.policyInstallCount);
    client.print(",transport_core0_failsafe="); client.print(transport.core0FailsafeCount);
    client.print(",profile_transitions="); client.print(transport.profileTransitionCount);
    client.print(",profile_failures="); client.print(transport.profileFailureCount);
    client.print(",profile_bbm_count="); client.print(transport.profileBreakBeforeMakeCount);
    client.print(",profile_last_us="); client.print(transport.lastProfileDurationUs);
    client.print(",profile_max_us="); client.print(transport.maxProfileDurationUs);
    client.print(",profile_clear_last_us="); client.print(transport.lastProfileClearDurationUs);
    client.print(",profile_clear_max_us="); client.println(transport.maxProfileClearDurationUs);
    return;
  }

  if (upper == "STATE?") {
    for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
      client.print("CH");
      client.print(ch + 1);
      client.print("=");
      client.print(hex16(channelMask[ch]));
      client.print(",");
      client.print(calculateOutputResistanceText(ch, channelMask[ch]));
      if (ch < CHANNEL_COUNT - 1) {
        client.print(";");
      }
    }
    client.println();
    return;
  }

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

