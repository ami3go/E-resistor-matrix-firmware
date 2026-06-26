/**
 * @file scpi_server.cpp
 * @brief SCPI TCP command help, parser, command executor, and calibration-table query implementation.
 */

#include "app.h"

// 12_scpi_server.ino
// Split from rp2040_w5500_resistor_matrix_v1_safety_logic.ino.
// Keep all files in the same Arduino sketch folder.

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

  if (ch < 1 || ch > 8) {
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
 * @brief Print one resistor value from the runtime calibration table.
 * @param client Connected TCP client used for the response.
 * @param resistanceText Function parameter.
 * @return Result value; for bool, true means the operation succeeded.
 */
static void scpiPrintOhmsFromConfig(WiFiClient& client, const char* resistanceText) {
  double ohms = 0.0;
  if (parseResistanceOhms(resistanceText, ohms)) {
    client.print(String(ohms, 6));
  } else {
    // Keep the table parseable even if a user-uploaded calibration file has
    // TODO/NC/N/A in a cell. The PC driver can reject NaN entries.
    client.print("NaN");
  }
}

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

  for (uint8_t i = 0; i < BIT_COUNT; i++) {
    RuntimeResistorInfo* info = &channelResistorTable[channelIndex][i];

    if (i > 0) {
      client.print(";");
    }

    client.print(info->bit);
    client.print(",");
    client.print(info->mosfet_name);
    client.print(",");
    scpiPrintOhmsFromConfig(client, info->nominal_resistance);
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
  if (ch < 1 || ch > 8) {
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
    client.print(",min_ohm=");
    client.print(String(safetyMinOhm, 3));
    client.print(",max_ohm=");
    client.print(String(safetyMaxOhm, 3));
    client.print(",expert=");
    client.print(safetyExpertMode ? "1" : "0");
    client.print(",core1_ready=");
    client.print(core1EngineReady ? "1" : "0");
    client.print(",core1_cmds=");
    client.print((uint32_t)core1CommandCounter);
    client.print(",core1_overflows=");
    client.println((uint32_t)core1QueueOverflowCounter);
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
    forceAllOff();
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
      setLastError("-350,\"Queue overflow\"");
      client.println("ERR,-350,\"Apply failed\"");
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
      scpiClient.println("E-Resistor SCPI ready");
      Serial.println("SCPI client connected");
    }
    return;
  }

  while (scpiClient.available() > 0) {
    char c = char(scpiClient.read());

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
      setLastError("-350,\"Input buffer overflow\"");
      scpiClient.println("ERR,-350,\"Input buffer overflow\"");
    }
  }
}

