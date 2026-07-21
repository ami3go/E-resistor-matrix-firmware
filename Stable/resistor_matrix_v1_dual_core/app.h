/**
 * @file app.h
 * @brief Shared declarations, global state, constants, and public module interfaces for the RP2040 resistor matrix firmware.
 *
 * This header is intentionally central in the Arduino prototype so that all
 * translation units share the same pinout, service objects, public APIs, and
 * dual-core command engine declarations.
 */

#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <W5500lwIP.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <LittleFS.h>
#include <Updater.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <pico/sync.h>

#include "board_config.h"
#include "hardware_config.h"
#include "core_transport_types.h"

// Centralized factory network defaults. Runtime values may be replaced by
// /network.csv, but every reset/default UI path must use these definitions.
inline constexpr uint8_t DEFAULT_DEVICE_IP_OCTETS[4] = {192, 168, 0, 55};
inline constexpr uint8_t DEFAULT_DEVICE_DNS_OCTETS[4] = {0, 0, 0, 0};
inline constexpr uint8_t DEFAULT_DEVICE_GATEWAY_OCTETS[4] = {0, 0, 0, 0};
inline constexpr uint8_t DEFAULT_DEVICE_SUBNET_OCTETS[4] = {255, 255, 255, 0};
inline constexpr const char* DEFAULT_DEVICE_IP_TEXT = "192.168.0.55";

#ifndef ERESISTOR_LOG_LEVEL
#define ERESISTOR_LOG_LEVEL 1
#endif

// ============================================================
// Firmware identity
// ============================================================
inline constexpr const char* FIRMWARE_NAME = "E-Resistor";
inline constexpr const char* FIRMWARE_VENDOR = "OpenBench";
inline constexpr const char* FIRMWARE_VERSION = "0.7.0";
inline constexpr const char* FIRMWARE_BUILD_DATE = __DATE__;
inline constexpr const char* FIRMWARE_BUILD_TIME = __TIME__;


// ============================================================
// Dual-core command transport and numeric events
// ============================================================
void initCoreCommandEngine();
bool initCore1EventQueue();
bool core1EmitEvent(Core1EventCode code, FirmwareLogLevel level, uint8_t channelIndex,
                    uint16_t mask, uint16_t detail, uint32_t durationUs, uint32_t sequence = 0);
bool popCore1Event(Core1Event& event);
void drainCore1Events();
const char* core1EventCodeText(Core1EventCode code);

/** @brief Temporary parsed calibration entry used only at text import boundaries. */
struct ParsedResistorInfo {
  uint8_t bit;
  float resistanceOhm;
};

/** @brief Detailed result from a read-only nearest-mask target calculation. */
struct TargetSearchResult {
  double requestedOhm;
  uint16_t mask;
  double calculatedOhm;
  double absoluteErrorOhm;
  double errorPercent;
  uint32_t candidatesVisited;
  uint32_t elapsedUs;
  bool timedOut;
  bool cancelled;
};

/** @brief WS2812 status indicator operating mode. */
enum LedMode : uint8_t {
  LED_BOOT,
  LED_OK,
  LED_ACTIVITY,
  LED_FAULT,
  LED_IDENTIFY_BLUE
};

// ============================================================
// Global state declarations
// ============================================================
extern Adafruit_NeoPixel heartbeatPixel;
extern LedMode ledMode;
extern LedMode ledModeBeforeIdentify;
extern bool ledPhase;
extern uint32_t lastLedUpdateMs;
extern uint32_t ledIdentifyUntilMs;
extern uint8_t ledIdentifyPatternStep;

extern IPAddress DEVICE_IP;
extern IPAddress DEVICE_DNS;
extern IPAddress DEVICE_GATEWAY;
extern IPAddress DEVICE_SUBNET;

extern Wiznet5500lwIP eth;
extern WebServer server;
extern WiFiServer scpiServer;
extern WiFiClient scpiClient;

extern char scpiLine[160];
extern size_t scpiLineLen;
extern bool scpiDiscardUntilNewline;

extern float channelResistorOhms[CHANNEL_COUNT][BIT_COUNT];

extern bool ethernetFault;
extern bool littleFsReady;
extern uint8_t calibrationSavedMask;
extern uint8_t calibrationLoadedMask;
extern uint8_t calibrationLoadErrorMask;
extern uint8_t w5500Version;

extern uint16_t channelMask[CHANNEL_COUNT];
extern uint32_t applyCounter[CHANNEL_COUNT];

// High-level safety state. Pinout is unchanged.
extern bool shiftRegistersReady;
extern bool outputsKnownSafe;
extern bool fatalSafeStateActive;

extern volatile bool core1EngineReady;
extern volatile bool core1OutputsReady;
extern volatile bool core1Fault;
extern volatile bool emergencyOffRequested;
extern volatile uint32_t core1HeartbeatMs;
extern volatile uint32_t core1LoopCounter;
extern volatile uint32_t core1CommandCounter;
extern volatile uint32_t core1QueueOverflowCounter;
extern volatile uint32_t core1LastCommandMs;
extern volatile uint32_t core1LoopMaxUs;
extern volatile uint32_t core1MinFreeStackBytes;
extern volatile uint32_t core1EventCounter;
extern volatile uint32_t core1EventDropCounter;

extern uint32_t targetSearchLastCandidates;
extern uint32_t targetSearchLastElapsedUs;
extern uint32_t targetSearchTimeoutCount;
extern uint32_t targetSearchCancelCount;

extern char statusText[128];
extern char lastError[128];
extern String deviceSerialNumber;

extern uint32_t runtimeWindowStartMs;
extern uint64_t runtimeBusyAccumUs;
extern uint32_t runtimeLoopCountWindow;
extern uint32_t runtimeLoopMaxUs;
extern float runtimeCore0LoadPct;
extern float runtimeLoopsPerSecond;
extern uint32_t httpRequestCount;
extern uint32_t scpiCommandCount;
extern uint32_t bootMillis;

extern double safetyMinOhm[CHANNEL_COUNT];
extern double safetyMaxOhm[CHANNEL_COUNT];
extern uint8_t safetyMaxActiveBits[CHANNEL_COUNT];
extern bool safetyExpertMode;

extern char eventLog[32][128];
extern uint8_t eventLogCount;
extern uint8_t eventLogHead;
extern char lastScpiCommand[96];
extern bool firmwareUpdateInProgress;
extern bool firmwareUpdateSucceeded;
extern char firmwareUpdateStatus[160];


// Utility
/**
 * @brief Ip To String.
 * @param ip Function parameter.
 * @return Result value; for bool, true means the operation succeeded.
 */
String ipToString(const IPAddress& ip);
/**
 * @brief Make Board Serial Number.
 * @return Result value; for bool, true means the operation succeeded.
 */
String makeBoardSerialNumber();
/**
 * @brief Parse Ip Address Text.
 * @param text Function parameter.
 * @param out Function parameter.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool parseIpAddressText(const String& text, IPAddress& out);
/**
 * @brief Network Config To Text.
 * @return Result value; for bool, true means the operation succeeded.
 */
String networkConfigToText();
/**
 * @brief Save Network Config To Little FS.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool saveNetworkConfigToLittleFS();
/**
 * @brief Load Network Config From Little FS.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool loadNetworkConfigFromLittleFS();
/**
 * @brief Hex16.
 * @param value Function parameter.
 * @return Result value; for bool, true means the operation succeeded.
 */
String hex16(uint16_t value);
/**
 * @brief Set Status.
 * @param text Function parameter.
 */
void setStatus(const char* text);
/**
 * @brief Set Last Error.
 * @param text Function parameter.
 */
void setLastError(const char* text);
/**
 * @brief Clear Last Error.
 */
void clearLastError();
/**
 * @brief Send No Cache Headers.
 */
void sendNoCacheHeaders();
/**
 * @brief Redirect To Root.
 */
void redirectToRoot();
/**
 * @brief Redirect To Settings.
 */
void redirectToSettings();
/**
 * @brief Enter a firmware safe state and request all outputs OFF.
 * @param reason Output buffer for a human-readable diagnostic message.
 */
void safeState(const char* reason);
/**
 * @brief Note Http Request.
 */
void noteHttpRequest();
/**
 * @brief Note Scpi Command.
 */
void noteScpiCommand();
/**
 * @brief Runtime Monitor Begin.
 */
void runtimeMonitorBegin();
/**
 * @brief Runtime Monitor Update.
 * @param busyUs Function parameter.
 */
void runtimeMonitorUpdate(uint32_t busyUs);
/**
 * @brief Get Heap Total Bytes.
 * @return Result value; for bool, true means the operation succeeded.
 */
uint32_t getHeapTotalBytes();
/**
 * @brief Get Heap Used Bytes.
 * @return Result value; for bool, true means the operation succeeded.
 */
uint32_t getHeapUsedBytes();
/**
 * @brief Get Heap Free Bytes.
 * @return Result value; for bool, true means the operation succeeded.
 */
uint32_t getHeapFreeBytes();
/**
 * @brief Get Heap Used Percent.
 * @return Result value; for bool, true means the operation succeeded.
 */
float getHeapUsedPercent();
/**
 * @brief Append Runtime Monitor Info.
 * @param html HTML string that receives generated markup.
 */
void appendRuntimeMonitorInfo(String& html);
/**
 * @brief Return firmware version/build string for UI and SCPI output.
 */
String firmwareVersionString();
/**
 * @brief Return a single-line firmware identity string.
 */
String firmwareIdentityString();

// Runtime resistor configuration
/**
 * @brief Copy compile-time resistor branch definitions into the mutable runtime table.
 */
void copyDefaultConfigToRuntime();
/** @brief Return the fixed MOSFET designator for a mask bit (bit 0 -> Q16). */
const char* mosfetNameForBit(uint8_t bit);
/** @brief Return one numeric runtime calibration value, or NaN for an invalid index. */
float getRuntimeResistanceOhms(uint8_t channelIndex, uint8_t bit);
/**
 * @brief Channel Config Path.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @return Result value; for bool, true means the operation succeeded.
 */
String channelConfigPath(uint8_t channelIndex);
/**
 * @brief Network Config Path.
 * @return Result value; for bool, true means the operation succeeded.
 */
String networkConfigPath();
/**
 * @brief Channel Config To Text.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @return Result value; for bool, true means the operation succeeded.
 */
String channelConfigToText(uint8_t channelIndex);

/**
 * @brief Return persistent LittleFS state for a per-channel calibration/config file.
 * @param channelIndex Zero-based channel index.
 * @param exists Output flag; true when /chN.csv exists in LittleFS.
 * @param sizeBytes Output file size in bytes when saved, otherwise 0.
 * @return true when the channel argument is valid.
 */
bool getChannelConfigStorageInfo(uint8_t channelIndex, bool& exists, size_t& sizeBytes);
/**
 * @brief Build a compact machine-readable list of per-channel calibration files.
 */
String calibrationFileListText();
/**
 * @brief Build a BEGIN/END-delimited text bundle of all active channel calibration tables.
 */
String allChannelConfigsToBundleText();
/**
 * @brief Validate and restore all eight channel calibration tables from one bundle.
 * @param text BEGIN/END-delimited bundle produced by allChannelConfigsToBundleText().
 * @param error Output buffer for a human-readable diagnostic message.
 * @param errorLen Size of the error output buffer.
 * @return true only when all eight tables were validated, saved, and activated.
 */
bool restoreAllChannelConfigsFromBundleText(const String& text, char* error, size_t errorLen);
/**
 * @brief Parse Csv Config Line.
 * @param line Function parameter.
 * @param out Function parameter.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool parseCsvConfigLine(const String& line, ParsedResistorInfo& out);
/**
 * @brief Parse Header Initializer Line.
 * @param line Function parameter.
 * @param out Function parameter.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool parseHeaderInitializerLine(const String& line, ParsedResistorInfo& out);
/**
 * @brief Parse Channel Config Text.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @param text Function parameter.
 * @param error Output buffer for a human-readable diagnostic message.
 * @param errorLen Output buffer for a human-readable diagnostic message.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool parseChannelConfigText(uint8_t channelIndex, const String& text, char* error, size_t errorLen);
/**
 * @brief Save Channel Config To Little FS.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool saveChannelConfigToLittleFS(uint8_t channelIndex);
/**
 * @brief Load Channel Config From Little FS.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool loadChannelConfigFromLittleFS(uint8_t channelIndex);
/**
 * @brief Load all per-channel runtime resistor configuration files from LittleFS.
 */
void loadAllRuntimeConfigs();
bool allChannelsHaveSavedCalibration();
String calibrationStorageStatusText();

// Resistance calculation helpers
/**
 * @brief Rebuild the cached conductance table used by fast resistance calculations.
 *
 * Call this after the runtime resistor table is populated or changed.  It
 * converts each configured branch resistance to conductance (1/R), so hot-path
 * calculations do not repeatedly parse String values.
 */
void rebuildConductanceCache();
/**
 * @brief Mark the cached conductance table stale.
 *
 * The next resistance calculation will rebuild the cache lazily.  Use this
 * after any direct edit to channelResistorOhms.
 */
void invalidateConductanceCache();
/**
 * @brief Parse Resistance Ohms.
 * @param text Function parameter.
 * @param ohms Function parameter.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool parseResistanceOhms(const char* text, double& ohms);
/**
 * @brief Format Resistance Ohms.
 * @param ohms Function parameter.
 * @return Result value; for bool, true means the operation succeeded.
 */
String formatResistanceOhms(double ohms);
/**
 * @brief Calculate Output Resistance Text.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @param mask 16-bit resistance switch mask.
 * @return Result value; for bool, true means the operation succeeded.
 */
String calculateOutputResistanceText(uint8_t channelIndex, uint16_t mask);
/**
 * @brief Calculate the equivalent resistance of all active branches in one channel mask.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @param mask 16-bit resistance switch mask.
 * @param outOhms Function parameter.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool calculateEquivalentOhms(uint8_t channelIndex, uint16_t mask, double& outOhms);
/**
 * @brief Count Active Bits16.
 * @param mask 16-bit resistance switch mask.
 * @return Result value; for bool, true means the operation succeeded.
 */
uint8_t countActiveBits16(uint16_t mask);
/**
 * @brief Validate a mask against the configured minimum resistance and maximum active-bit limits.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @param mask 16-bit resistance switch mask.
 * @param reason Output buffer for a human-readable diagnostic message.
 * @param reasonLen Output buffer for a human-readable diagnostic message.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool checkMaskSafety(uint8_t channelIndex, uint16_t mask, char* reason, size_t reasonLen);
/**
 * @brief Resistance Css Class.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @param mask 16-bit resistance switch mask.
 * @return Result value; for bool, true means the operation succeeded.
 */
String resistanceCssClass(uint8_t channelIndex, uint16_t mask);
/**
 * @brief Safety Config Path.
 * @return Result value; for bool, true means the operation succeeded.
 */
String safetyConfigPath();
/**
 * @brief Calibration Meta Path.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @return Result value; for bool, true means the operation succeeded.
 */
String calibrationMetaPath(uint8_t channelIndex);
/**
 * @brief Append Log Event.
 * @param text Function parameter.
 */
void appendLogEvent(const char* text);
/**
 * @brief Sanitize Name.
 * @param name Function parameter.
 * @return Result value; for bool, true means the operation succeeded.
 */
String sanitizeName(String name);
/**
 * @brief Profile Path From Name.
 * @param name Function parameter.
 * @return Result value; for bool, true means the operation succeeded.
 */
String profilePathFromName(String name);
/**
 * @brief Save Safety Config To Little FS.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool saveSafetyConfigToLittleFS();
/**
 * @brief Load Safety Config From Little FS.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool loadSafetyConfigFromLittleFS();
/**
 * @brief Search masks locally on the RP2040 to find the nearest output resistance.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @param targetOhm Function parameter.
 * @param bestMask 16-bit resistance switch mask.
 * @param bestOhm Function parameter.
 * @param bestErrorPercent Output buffer for a human-readable diagnostic message.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool calculateNearestMaskForTarget(uint8_t channelIndex, double targetOhm, TargetSearchResult& result, uint32_t deadlineUs = 2000000UL);
bool findNearestMaskForTarget(uint8_t channelIndex, double targetOhm, uint16_t& bestMask, double& bestOhm, double& bestErrorPercent);
void requestTargetSearchCancel();
void clearTargetSearchCancel();
/**
 * @brief Profile List Options.
 * @return Result value; for bool, true means the operation succeeded.
 */
String profileListOptions();
/**
 * @brief Load Profile Masks.
 * @param profileName Size of the associated output buffer in bytes.
 * @param masks 16-bit resistance switch mask.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool loadProfileMasks(String profileName, uint16_t masks[CHANNEL_COUNT]);
/**
 * @brief Save Current Profile.
 * @param profileName Size of the associated output buffer in bytes.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool saveCurrentProfile(String profileName);
/**
 * @brief Append Profile Manager.
 * @param html HTML string that receives generated markup.
 */
void appendProfileManager(String& html);
/**
 * @brief Append Safety Summary Card.
 * @param html HTML string that receives generated markup.
 */
void appendSafetySummaryCard(String& html);
/**
 * @brief Append one combined CH1..CH8 resistor calibration table.
 * @param html HTML string that receives generated markup.
 */
void appendCombinedChannelResistorTable(String& html);
/**
 * @brief Append Live State Script.
 * @param html HTML string that receives generated markup.
 */
void appendLiveStateScript(String& html);
/**
 * @brief Append one channel bit-status LED row to a generated HTML page.
 * @param html HTML string that receives generated markup.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @param mask 16-bit resistance switch mask.
 */
void appendBitIndicator(String& html, uint8_t channelIndex, uint16_t mask);

// Status LED
/**
 * @brief Ws2812 Set.
 * @param r Function parameter.
 * @param g Function parameter.
 * @param b Function parameter.
 */
void ws2812Set(uint8_t r, uint8_t g, uint8_t b);
/**
 * @brief Initialize the WS2812 heartbeat pixel.
 */
void heartbeatBegin();
/**
 * @brief Set Led Mode.
 * @param mode Function parameter.
 */
void setLedMode(LedMode mode);
/**
 * @brief Start a temporary bright-blue identify blink pattern on the WS2812 LED.
 * @param durationMs Duration of the identify pattern in milliseconds.
 */
void startIdentifyLedBlink(uint32_t durationMs);
/**
 * @brief Update the WS2812 indicator according to the current LED mode.
 */
void updateHeartbeat();

// W5500 register helpers
/**
 * @brief Write one W5500 common-register byte through SPI.
 * @param address Function parameter.
 * @param value Function parameter.
 * @param spiHz Function parameter.
 */
void w5500WriteCommonReg(uint16_t address, uint8_t value, uint32_t spiHz);
/**
 * @brief Read one W5500 common-register byte through SPI.
 * @param address Function parameter.
 * @param spiHz Function parameter.
 * @return Result value; for bool, true means the operation succeeded.
 */
uint8_t w5500ReadCommonReg(uint16_t address, uint32_t spiHz);
/**
 * @brief Issue W5500 software reset and verify VERSIONR communication.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool w5500SoftwareResetAndProbe();

// Dual-core command engine. Core 0 owns policy calculation and text formatting.
void initCoreCommandEngine();
bool waitForCore1Startup(uint32_t timeoutMs = CORE_STARTUP_TIMEOUT_MS);
bool refreshCore0OutputMirror();
bool readCoreOutputSnapshot(CoreOutputSnapshot& snapshot);
bool submitCoreCommandWait(CoreCommand& command, CoreResult& result,
                           uint32_t timeoutMs = CORE_COMMAND_TIMEOUT_MS);
bool requestSetChannelMask(uint8_t channelIndex, uint16_t mask, char* reason, size_t reasonLen);
bool requestSetAllMasks(const uint16_t masks[CHANNEL_COUNT], char* reason, size_t reasonLen);
bool requestAllOff(char* reason, size_t reasonLen);
bool installCore1PolicySnapshot(char* reason = nullptr, size_t reasonLen = 0);
void getCoreTransportDiagnostics(CoreTransportDiagnostics& diagnostics);
void coreTransportKickCore0Heartbeat();
uint32_t coreTransportInvalidateGeneration();

// Core-0 safe wrappers. HTTP and SCPI call only these functions.
bool applyChannelMask(uint8_t channelIndex, uint16_t newMask);
bool applyAllMasksSafely(const uint16_t masks[CHANNEL_COUNT], char* reason, size_t reasonLen);
bool forceAllOff(char* reason = nullptr, size_t reasonLen = 0);

#ifdef ERESISTOR_TEST_MODE
void core1TestSetNextCommandDelayMs(uint32_t delayMs);
void core1TestPauseProcessingMs(uint32_t pauseMs);
void core1TestInvalidateNextCommandGeneration();
#endif

// Ethernet startup
/**
 * @brief Start W5500 Ethernet using the configured static IPv4 settings.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool startEthernetStatic();

// Parse helpers
/**
 * @brief Parse Channel.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool parseChannel(uint8_t& channelIndex);
/**
 * @brief Parse Bit.
 * @param bitIndex Function parameter.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool parseBit(uint8_t& bitIndex);
/**
 * @brief Parse a 16-bit mask from hexadecimal text with or without 0x prefix.
 * @param s Function parameter.
 * @param mask 16-bit resistance switch mask.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool parseHex16String(String s, uint16_t& mask);
/**
 * @brief Parse Mask.
 * @param mask 16-bit resistance switch mask.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool parseMask(uint16_t& mask);

// HTTP page helpers
/**
 * @brief Append Common Page Header.
 * @param html HTML string that receives generated markup.
 * @param title Function parameter.
 */
void appendCommonPageHeader(String& html, const char* title);
/**
 * @brief Append Common Page Footer.
 * @param html HTML string that receives generated markup.
 */
void appendCommonPageFooter(String& html);

// LittleFS page helpers
/**
 * @brief U64 To String.
 * @param value Function parameter.
 * @return Result value; for bool, true means the operation succeeded.
 */
String u64ToString(uint64_t value);
/**
 * @brief Format Bytes Human.
 * @param bytes Function parameter.
 * @return Result value; for bool, true means the operation succeeded.
 */
String formatBytesHuman(uint64_t bytes);
/**
 * @brief Append one combined inventory of expected calibration files and all
 *        other stored LittleFS root files.
 * @param html HTML string that receives generated markup.
 */
void appendLittleFsCombinedFiles(String& html);
/**
 * @brief Append Little Fs Storage Info.
 * @param html HTML string that receives generated markup.
 */
void appendLittleFsStorageInfo(String& html);

// HTTP handlers
/**
 * @brief Handle Ping.
 */
void handlePing();
/**
 * @brief Return the current machine state as a compact text/JSON-like response.
 */
void handleState();
/**
 * @brief Handle Live State Page.
 */
void handleLiveStatePage();
/**
 * @brief Handle Root.
 */
void handleRoot();
/**
 * @brief Append Network Settings Form.
 * @param html HTML string that receives generated markup.
 */
void appendNetworkSettingsForm(String& html);
/**
 * @brief Handle Network.
 */
void handleNetwork();
/**
 * @brief Handle Network Save.
 */
void handleNetworkSave();
/**
 * @brief Delete saved Ethernet settings from LittleFS and restore defaults in RAM.
 */
void handleNetworkDelete();
/**
 * @brief Handle Runtime Page.
 */
void handleRuntimePage();
/**
 * @brief Handle Files Page.
 */
void handleFilesPage();
/**
 * @brief Delete one root-level LittleFS file from the Files tab.
 */
void handleFileDelete();
/**
 * @brief Handle Settings.
 */
void handleSettings();
/**
 * @brief Handle Upload Config.
 */
void handleUploadConfig();
/**
 * @brief Handle Upload Config Api.
 */
void handleUploadConfigApi();
/**
 * @brief Handle Download Config.
 */
void handleDownloadConfig();
/**
 * @brief Return machine-readable calibration file list for the GUI calibration tool.
 */
void handleCalibrationFilesApi();
/**
 * @brief Return all active calibration tables as a BEGIN/END-delimited text bundle.
 */
void handleCalibrationDownloadAll();
/** @brief Download all calibration tables as a browser attachment. */
void handleCalibrationDownloadAllFile();
/** @brief Import and restore all calibration tables from one uploaded text bundle. */
void handleCalibrationImportAll();
/**
 * @brief Handle Toggle Bit.
 */
void handleToggleBit();
/**
 * @brief Handle Target Apply.
 */
void handleTargetApply();
/**
 * @brief Handle the control-page identify LED request.
 */
void handleIdentifyLed();
/**
 * @brief Handle Profiles Page.
 */
void handleProfilesPage();
/**
 * @brief Handle Profile Save.
 */
void handleProfileSave();
/**
 * @brief Handle Profile Apply.
 */
void handleProfileApply();
/**
 * @brief Handle Profile Delete.
 */
void handleProfileDelete();
/**
 * @brief Handle Safety Page.
 */
void handleSafetyPage();
/**
 * @brief Handle Safety Save.
 */
void handleSafetySave();
/**
 * @brief Render the browser-visible SCPI command reference page.
 */
void handleScpiPage();
/**
 * @brief Handle Log Page.
 */
void handleLogPage();
/**
 * @brief Download the current in-memory event log as a text file.
 */
void handleLogDownload();
/**
 * @brief Handle Backup Page.
 */
void handleBackupPage();
/**
 * @brief Handle Backup Download.
 */
void handleBackupDownload();
/**
 * @brief Delete Files By Prefix.
 * @param prefix Function parameter.
 */
void deleteFilesByPrefix(const char* prefix);
/**
 * @brief Handle Factory Reset.
 */
void handleFactoryReset();
/**
 * @brief Handle Watchdog Page.
 */
void handleWatchdogPage();
/**
 * @brief Handle Calibration Meta Save.
 */
void handleCalibrationMetaSave();
/**
 * @brief Delete calibration metadata for one channel.
 */
void handleCalibrationMetaDelete();
/**
 * @brief Handle Set.
 */
void handleSet();
/**
 * @brief Handle All Off.
 */
void handleAllOff();
/**
 * @brief Render firmware version and binary update upload page.
 */
void handleFirmwarePage();
/**
 * @brief Finalize a firmware binary upload request.
 */
void handleFirmwareUpdateDone();
/**
 * @brief Stream firmware binary upload chunks into the Arduino-Pico Updater.
 */
void handleFirmwareUpdateUpload();
/**
 * @brief Handle Not Found.
 */
void handleNotFound();
/**
 * @brief Register all HTTP routes and start the web server.
 */
void setupHttpServer();

// SCPI server
/**
 * @brief Print the supported SCPI command list to an active TCP client.
 * @param client Connected TCP client used for the response.
 */
void scpiPrintHelp(WiFiClient& client);
/**
 * @brief Parse SCPI channel-command aliases and return a zero-based channel index.
 * @param cmd Function parameter.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @param rest Function parameter.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool parseScpiChannelCommand(String cmd, uint8_t& channelIndex, String& rest);
/**
 * @brief Parse and execute one received SCPI command line.
 * @param rawLine Function parameter.
 * @param client Connected TCP client used for the response.
 */
void processScpiLine(const char* rawLine, WiFiClient& client);
/**
 * @brief Start the raw TCP SCPI server.
 */
void setupScpiServer();
/**
 * @brief Accept SCPI clients and process line-oriented commands.
 */
void handleScpiServer();

// Arduino entry points are implemented in setup_loop.cpp.
/**
 * @brief Arduino Core 0 startup entry point for communications and non-real-time services.
 */
void setup();
/**
 * @brief Arduino Core 0 loop for HTTP, SCPI, heartbeat, and monitoring tasks.
 */
void loop();
/**
 * @brief Arduino Core 1 startup entry point for deterministic output control.
 */
void setup1();
/**
 * @brief Arduino Core 1 loop that runs the hardware command engine.
 */
void loop1();
