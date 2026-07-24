/** @file scpi_api.h @brief Fixed-buffer SCPI registry, parser and TCP service interfaces. */
#pragma once
#include "utility_api.h"
#include "calibration_api.h"
#include "core_api.h"

enum class ScpiCommandId : uint8_t {
  Unknown = 0,
  Help,
  IdnQuery,
  SerialQuery,
  SystemVersionQuery,
  FirmwareVersionQuery,
  FirmwareBuildQuery,
  ClearStatus,
  ErrorQuery,
  ErrorClear,
  StateQuery,
  SystemStatusQuery,
  CoreTransportQuery,
  CoreSnapshotQuery,
  CoreProfileQuery,
  SerialDiagnosticQuery,
  UsbDiagnosticQuery,
  CalibrationStatusQuery,
  AllOff
};

struct ScpiCommandDefinition {
  const char* command;
  ScpiCommandId id;
  const char* helpText;
  bool alias;
};

const ScpiCommandDefinition* scpiCommandRegistry(size_t& count);
ScpiCommandId scpiLookupExactCommand(const char* normalizedCommand);
void scpiPrintHelp(WiFiClient& client);
bool normalizeScpiLine(const char* rawLine, char* normalized, size_t normalizedLength);
bool parseScpiChannelCommandFixed(const char* command, uint8_t& channelIndex, const char*& tail);
bool parseScpiChannelCommand(String cmd, uint8_t& channelIndex, String& rest);
void processScpiLine(const char* rawLine, WiFiClient& client);
void setupScpiServer();
void handleScpiServer();
