/**
 * @file scpi_command_registry.cpp
 * @brief Fixed-buffer Gate 5 SCPI command registry and shared help source.
 */
#include "scpi_api.h"

static const ScpiCommandDefinition kCommands[] = {
  {"HELP?", ScpiCommandId::Help, "List commands generated from this registry.", false},
  {"HELP", ScpiCommandId::Help, nullptr, true},
  {"*IDN?", ScpiCommandId::IdnQuery, "Read vendor, model, serial and firmware version.", false},
  {"SYST:SER?", ScpiCommandId::SerialQuery, "Read board serial number.", false},
  {"SYSTEM:SERIAL?", ScpiCommandId::SerialQuery, nullptr, true},
  {"SERIAL?", ScpiCommandId::SerialQuery, nullptr, true},
  {"SER?", ScpiCommandId::SerialQuery, nullptr, true},
  {"SYST:VERS?", ScpiCommandId::SystemVersionQuery, "Read firmware version.", false},
  {"SYSTEM:VERSION?", ScpiCommandId::SystemVersionQuery, nullptr, true},
  {"FIRM:VERS?", ScpiCommandId::FirmwareVersionQuery, "Read firmware version alias.", false},
  {"FIRMWARE:VERSION?", ScpiCommandId::FirmwareVersionQuery, nullptr, true},
  {"VERS?", ScpiCommandId::FirmwareVersionQuery, nullptr, true},
  {"FIRM:BUILD?", ScpiCommandId::FirmwareBuildQuery, "Read build date and time.", false},
  {"FIRMWARE:BUILD?", ScpiCommandId::FirmwareBuildQuery, nullptr, true},
  {"*CLS", ScpiCommandId::ClearStatus, "Clear the SCPI error queue.", false},
  {"SYST:ERR:CLEAR", ScpiCommandId::ErrorClear, "Clear the SCPI error queue alias.", false},
  {"SYSTEM:ERROR:CLEAR", ScpiCommandId::ErrorClear, nullptr, true},
  {"SYST:ERR?", ScpiCommandId::ErrorQuery, "Read and clear the last error.", false},
  {"SYSTEM:ERROR?", ScpiCommandId::ErrorQuery, nullptr, true},
  {"STATE?", ScpiCommandId::StateQuery, "Read all eight masks and calculated resistances.", false},
  {"SYST:STAT?", ScpiCommandId::SystemStatusQuery, "Read system, safety, HTTP and transport status.", false},
  {"SYSTEM:STATUS?", ScpiCommandId::SystemStatusQuery, nullptr, true},
  {"SYST:CORE:TRANSPORT?", ScpiCommandId::CoreTransportQuery, "Read bounded command-transport diagnostics.", false},
  {"SYSTEM:CORE:TRANSPORT?", ScpiCommandId::CoreTransportQuery, nullptr, true},
  {"SYST:CORE:SNAPSHOT?", ScpiCommandId::CoreSnapshotQuery, "Read the coherent Core 1 output snapshot.", false},
  {"SYSTEM:CORE:SNAPSHOT?", ScpiCommandId::CoreSnapshotQuery, nullptr, true},
  {"SYST:CORE:PROFILE?", ScpiCommandId::CoreProfileQuery, "Read Gate 4+ profile-transition diagnostics.", false},
  {"SYSTEM:CORE:PROFILE?", ScpiCommandId::CoreProfileQuery, nullptr, true},
  {"SYST:DIAG:SERIAL?", ScpiCommandId::SerialDiagnosticQuery, "Emit a deterministic USB serial event.", false},
  {"SYSTEM:DIAGNOSTIC:SERIAL?", ScpiCommandId::SerialDiagnosticQuery, nullptr, true},
  {"SYST:DIAG:USB?", ScpiCommandId::UsbDiagnosticQuery, "Read USB CDC start and host state.", false},
  {"SYSTEM:DIAGNOSTIC:USB?", ScpiCommandId::UsbDiagnosticQuery, nullptr, true},
  {"CAL:STATUS?", ScpiCommandId::CalibrationStatusQuery, "Read saved/loaded/error calibration masks.", false},
  {"CALIBRATION:STATUS?", ScpiCommandId::CalibrationStatusQuery, nullptr, true},
  {"ALL:OFF", ScpiCommandId::AllOff, "Force every resistor channel OFF.", false},
  {":ALL:OFF", ScpiCommandId::AllOff, nullptr, true},
  {"OUTP:ALL OFF", ScpiCommandId::AllOff, "Force every resistor channel OFF alias.", false},
  {"OUTPUT:ALL OFF", ScpiCommandId::AllOff, nullptr, true},

  // Registry documentation entries for parameterized commands. These are
  // intentionally not exact-match dispatch entries.
  {"ROUT:ALL:MASK <M1>,...,<M8>", ScpiCommandId::Unknown, "Apply one atomic eight-channel profile.", false},
  {"CH<N>:MASK <HEX>", ScpiCommandId::Unknown, "Set one channel mask.", false},
  {"CH<N>:MASK?", ScpiCommandId::Unknown, "Read one channel mask.", false},
  {"CH<N>:RES?", ScpiCommandId::Unknown, "Read calculated output resistance.", false},
  {"CH<N>:TARGET:CALC? <OHM>", ScpiCommandId::Unknown, "Calculate nearest safe mask without actuation.", false},
  {"CH<N>:CONF?", ScpiCommandId::Unknown, "Read one channel calibration table as CSV.", false},
  {"CAL:RES?", ScpiCommandId::Unknown, "Read all compact calibration branch values.", false},
  {"CAL:RES? CH<N>", ScpiCommandId::Unknown, "Read one compact calibration table.", false},
  {"CAL:FILES?", ScpiCommandId::Unknown, "List saved calibration files.", false},
  {"CAL:FILE? CH<N>", ScpiCommandId::Unknown, "Download one calibration CSV.", false},
  {"CAL:ALL:FILES?", ScpiCommandId::Unknown, "Download all calibration CSV tables.", false},
};

const ScpiCommandDefinition* scpiCommandRegistry(size_t& count) {
  count = sizeof(kCommands) / sizeof(kCommands[0]);
  return kCommands;
}

bool normalizeScpiLine(const char* rawLine, char* normalized, size_t normalizedLength) {
  if (!rawLine || !normalized || normalizedLength < 2U) return false;
  size_t writeIndex = 0;
  bool pendingSpace = false;
  const unsigned char* p = reinterpret_cast<const unsigned char*>(rawLine);
  while (*p && isspace(*p)) ++p;
  for (; *p; ++p) {
    if (isspace(*p)) {
      pendingSpace = writeIndex > 0U;
      continue;
    }
    if (pendingSpace) {
      if (writeIndex + 1U >= normalizedLength) return false;
      normalized[writeIndex++] = ' ';
      pendingSpace = false;
    }
    if (writeIndex + 1U >= normalizedLength) return false;
    normalized[writeIndex++] = char(toupper(*p));
  }
  normalized[writeIndex] = '\0';
  return writeIndex > 0U;
}

ScpiCommandId scpiLookupExactCommand(const char* normalizedCommand) {
  if (!normalizedCommand) return ScpiCommandId::Unknown;
  size_t count = 0;
  const ScpiCommandDefinition* entries = scpiCommandRegistry(count);
  for (size_t i = 0; i < count; ++i) {
    if (entries[i].id != ScpiCommandId::Unknown && strcmp(entries[i].command, normalizedCommand) == 0) {
      return entries[i].id;
    }
  }
  return ScpiCommandId::Unknown;
}

bool parseScpiChannelCommandFixed(const char* command, uint8_t& channelIndex, const char*& tail) {
  if (!command) return false;
  const char* p = command;
  while (*p == ':') ++p;
  if (strncmp(p, "ROUTE:", 6) == 0) p += 6;
  else if (strncmp(p, "ROUT:", 5) == 0) p += 5;
  if (strncmp(p, "CHANNEL", 7) == 0) p += 7;
  else if (strncmp(p, "CHAN", 4) == 0) p += 4;
  else if (strncmp(p, "CH", 2) == 0) p += 2;
  else return false;
  if (!isdigit(static_cast<unsigned char>(*p))) return false;
  unsigned channel = 0;
  while (isdigit(static_cast<unsigned char>(*p))) {
    channel = channel * 10U + unsigned(*p - '0');
    ++p;
  }
  if (channel < 1U || channel > CHANNEL_COUNT) return false;
  while (*p == ':' || *p == ' ') ++p;
  channelIndex = uint8_t(channel - 1U);
  tail = p;
  return true;
}

void scpiPrintHelp(WiFiClient& client) {
  client.println("Commands (generated from Gate 5 registry):");
  size_t count = 0;
  const ScpiCommandDefinition* entries = scpiCommandRegistry(count);
  for (size_t i = 0; i < count; ++i) {
    if (entries[i].alias || !entries[i].helpText) continue;
    client.print(entries[i].command);
    client.print(" - ");
    client.println(entries[i].helpText);
  }
#ifdef ERESISTOR_TEST_MODE
  client.println("SYST:TEST:MODE? - returns 1 for fault-injection builds");
#endif
}
