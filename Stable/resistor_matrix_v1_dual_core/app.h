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
#include <ctype.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <pico/sync.h>

#include "board_config.h"

// ============================================================
// Ethernet pins - unchanged
// ============================================================
inline constexpr uint8_t ETH_MISO = 0;
inline constexpr uint8_t ETH_CS   = 1;
inline constexpr uint8_t ETH_SCK  = 2;
inline constexpr uint8_t ETH_MOSI = 3;
inline constexpr uint8_t ETH_INT  = 4;

// ============================================================
// Shift-register pins - unchanged
// ============================================================
inline constexpr uint8_t SR_DATA  = 11;
inline constexpr uint8_t SR_CLOCK = 12;
inline constexpr uint8_t SR_RESET = 15;  // active low

inline constexpr uint8_t CHANNEL_COUNT = 8;
inline constexpr uint8_t BIT_COUNT = 16;

inline constexpr uint8_t SR_LATCH_PINS[CHANNEL_COUNT] = {
  5,   // CH1
  6,   // CH2
  7,   // CH3
  8,   // CH4
  9,   // CH5
  10,  // CH6
  13,  // CH7
  14   // CH8
};

// ============================================================
// WS2812 heartbeat - unchanged
// ============================================================
inline constexpr uint8_t WS2812_PIN = 16;
inline constexpr uint8_t WS2812_COUNT = 1;

// ============================================================
// Timing / networking
// ============================================================
inline constexpr uint32_t ETH_INIT_SPI_HZ    = 1000000UL;
inline constexpr uint32_t ETH_RUNTIME_SPI_HZ = 4000000UL;  // keep 4 MHz while debugging

inline constexpr uint16_t SR_CLOCK_HALF_PERIOD_US = 10;
inline constexpr uint16_t SR_LATCH_PULSE_US       = 10;
inline constexpr uint16_t BREAK_BEFORE_MAKE_MS    = 2;

inline constexpr uint16_t HTTP_TCP_PORT = 80;
inline constexpr uint16_t SCPI_TCP_PORT = 5025;


// ============================================================
// Dual-core command engine
/**
 * @brief Initialize the dual-core command engine and its RP2040 hardware spin lock.
 */
void initCoreCommandEngine();
// Core 0 owns communications; Core 1 owns physical outputs.
// ============================================================
inline constexpr uint8_t CORE_COMMAND_QUEUE_DEPTH = 8;
inline constexpr uint8_t CORE_RESPONSE_QUEUE_DEPTH = 8;
inline constexpr uint32_t CORE_COMMAND_TIMEOUT_MS = 1000UL;

/** @brief Command identifiers transported from Core 0 communication handlers to the Core 1 hardware engine. */
enum CoreCommandType : uint8_t {
  CORE_CMD_NONE = 0,
  CORE_CMD_SET_MASK,
  CORE_CMD_SET_ALL_MASKS,
  CORE_CMD_CLEAR_ALL,
  CORE_CMD_GET_MASK,
  CORE_CMD_GET_ALL_MASKS
};

/** @brief SCPI-compatible response/status codes returned by the Core 1 command engine. */
enum CoreResponseStatus : int16_t {
  CORE_RESP_OK = 0,
  CORE_RESP_BUSY = -300,
  CORE_RESP_TIMEOUT = -501,
  CORE_RESP_INVALID_ARGUMENT = -101,
  CORE_RESP_REJECTED = -222,
  CORE_RESP_IO_ERROR = -500
};

/** @brief Command payload sent from Core 0 to Core 1 for deterministic hardware execution. */
struct CoreCommand {
  CoreCommandType type;
  uint32_t requestId;
  uint8_t channelIndex;       // 0..7 for per-channel commands
  uint16_t mask;
  uint16_t masks[CHANNEL_COUNT];
};

/** @brief Response payload returned by Core 1 after a command is accepted, rejected, or completed. */
struct CoreResponse {
  uint32_t requestId;
  CoreResponseStatus status;
  uint8_t channelIndex;
  uint16_t mask;
  uint16_t masks[CHANNEL_COUNT];
  char message[96];
};

/** @brief Runtime-editable resistor branch calibration entry for one channel/bit. */
struct RuntimeResistorInfo {
  uint8_t bit;
  char mosfet_name[8];
  char nominal_resistance[16];
};

/** @brief WS2812 status indicator operating mode. */
enum LedMode : uint8_t {
  LED_BOOT,
  LED_OK,
  LED_ACTIVITY,
  LED_FAULT
};

// ============================================================
// Global state declarations
// ============================================================
extern Adafruit_NeoPixel heartbeatPixel;
extern LedMode ledMode;
extern bool ledPhase;
extern uint32_t lastLedUpdateMs;

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

extern RuntimeResistorInfo channelResistorTable[CHANNEL_COUNT][BIT_COUNT];

extern bool ethernetFault;
extern bool littleFsReady;
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

extern double safetyMinOhm;
extern uint8_t safetyMaxActiveBits;
extern bool safetyExpertMode;

extern char eventLog[32][128];
extern uint8_t eventLogCount;
extern uint8_t eventLogHead;
extern char lastScpiCommand[96];


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

// Runtime resistor configuration
/**
 * @brief Copy compile-time resistor branch definitions into the mutable runtime table.
 */
void copyDefaultConfigToRuntime();
/**
 * @brief Get Runtime Resistor Info For Bit.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @param bit Function parameter.
 * @return Result value; for bool, true means the operation succeeded.
 */
RuntimeResistorInfo* getRuntimeResistorInfoForBit(uint8_t channelIndex, uint8_t bit);
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
 * @brief Parse Csv Config Line.
 * @param line Function parameter.
 * @param out Function parameter.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool parseCsvConfigLine(const String& line, RuntimeResistorInfo& out);
/**
 * @brief Parse Header Initializer Line.
 * @param line Function parameter.
 * @param out Function parameter.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool parseHeaderInitializerLine(const String& line, RuntimeResistorInfo& out);
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

// Resistance calculation helpers
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
bool findNearestMaskForTarget(uint8_t channelIndex, double targetOhm, uint16_t& bestMask, double& bestOhm, double& bestErrorPercent);
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

// Dual-core command engine
/**
 * @brief Initialize the dual-core command engine and its RP2040 hardware spin lock.
 */
void initCoreCommandEngine();
/**
 * @brief Submit a command to Core 1 and wait for the matching response with a timeout.
 * @param command Command object to submit or process.
 * @param response Response object to fill or inspect.
 * @param timeoutMs Maximum time to wait before reporting timeout.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool submitCoreCommandWait(CoreCommand& command, CoreResponse& response, uint32_t timeoutMs = CORE_COMMAND_TIMEOUT_MS);
/**
 * @brief Request Core 1 to apply one 16-bit resistance mask to one channel.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @param mask 16-bit resistance switch mask.
 * @param reason Output buffer for a human-readable diagnostic message.
 * @param reasonLen Output buffer for a human-readable diagnostic message.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool requestSetChannelMask(uint8_t channelIndex, uint16_t mask, char* reason, size_t reasonLen);
/**
 * @brief Request Core 1 to apply eight channel masks after validating the full profile.
 * @param masks 16-bit resistance switch mask.
 * @param reason Output buffer for a human-readable diagnostic message.
 * @param reasonLen Output buffer for a human-readable diagnostic message.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool requestSetAllMasks(const uint16_t masks[CHANNEL_COUNT], char* reason, size_t reasonLen);
/**
 * @brief Request Core 1 to switch every output channel OFF.
 * @param reason Output buffer for a human-readable diagnostic message.
 * @param reasonLen Output buffer for a human-readable diagnostic message.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool requestAllOff(char* reason, size_t reasonLen);
/**
 * @brief Run one iteration of the Core 1 command-processing engine.
 */
void core1ProcessEngineOnce();

// Shift-register physical control. These functions are owned by Core 1.
/**
 * @brief Generate one shift-register clock pulse using the configured timing.
 */
void pulseClock();
/**
 * @brief Pulse exactly one channel latch pin.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 */
void pulseLatch(uint8_t channelIndex);
/**
 * @brief Pulse SRCLR low to clear the cascaded 74HC595 outputs.
 */
void pulseShiftRegisterClear();
/**
 * @brief Shift a 16-bit mask to the register chain with bit 0 transmitted first.
 * @param mask 16-bit resistance switch mask.
 */
void shiftMaskBit0First(uint16_t mask);
/**
 * @brief Shift and latch a mask into exactly one channel output register.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @param mask 16-bit resistance switch mask.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool latchMaskToChannel(uint8_t channelIndex, uint16_t mask);
/**
 * @brief Apply one channel mask with break-before-make sequencing on Core 1.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @param newMask 16-bit resistance switch mask.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool applyChannelMaskPhysical(uint8_t channelIndex, uint16_t newMask);
/**
 * @brief Validate and apply an eight-channel mask set using physical Core 1 switching.
 * @param masks 16-bit resistance switch mask.
 * @param reason Output buffer for a human-readable diagnostic message.
 * @param reasonLen Output buffer for a human-readable diagnostic message.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool applyAllMasksSafelyPhysical(const uint16_t masks[CHANNEL_COUNT], char* reason, size_t reasonLen);
/**
 * @brief Configure shift-register GPIO pins, pulse SRCLR, and force known-safe outputs.
 */
void setupShiftRegisters();
/**
 * @brief Physically latch 0x0000 into all eight output channels.
 */
void forceAllOffPhysical();

// Core-0 safe wrappers. HTTP and SCPI should call only these wrappers.
/**
 * @brief Core-0-safe wrapper that requests a per-channel mask change on Core 1.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @param newMask 16-bit resistance switch mask.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool applyChannelMask(uint8_t channelIndex, uint16_t newMask);
/**
 * @brief Core-0-safe wrapper that requests an all-channel mask profile on Core 1.
 * @param masks 16-bit resistance switch mask.
 * @param reason Output buffer for a human-readable diagnostic message.
 * @param reasonLen Output buffer for a human-readable diagnostic message.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool applyAllMasksSafely(const uint16_t masks[CHANNEL_COUNT], char* reason, size_t reasonLen);
/**
 * @brief Core-0-safe wrapper that requests an emergency/normal all-OFF transition.
 */
void forceAllOff();

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
 * @brief Append Little Fs Channel Config Status.
 * @param html HTML string that receives generated markup.
 */
void appendLittleFsChannelConfigStatus(String& html);
/**
 * @brief Append Little Fs Stored Files.
 * @param html HTML string that receives generated markup.
 */
void appendLittleFsStoredFiles(String& html);
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
 * @brief Handle Runtime Page.
 */
void handleRuntimePage();
/**
 * @brief Handle Files Page.
 */
void handleFilesPage();
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
 * @brief Handle Toggle Bit.
 */
void handleToggleBit();
/**
 * @brief Handle Target Apply.
 */
void handleTargetApply();
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
 * @brief Handle Set.
 */
void handleSet();
/**
 * @brief Handle All Off.
 */
void handleAllOff();
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
