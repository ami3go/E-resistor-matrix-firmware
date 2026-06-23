# Generated Firmware API Reference

This file was generated from the documented source package. It is provided as a readable reference even on systems where Doxygen is not installed.

## `core_command.cpp`

### `initCoreCommandEngine`

Initialize the dual-core command engine and its RP2040 hardware spin lock.

```cpp
void initCoreCommandEngine()
```

### `lockQueue`

Acquire the shared command/response queue lock.

```cpp
static uint32_t lockQueue(spin_lock_t* lock)
```

### `unlockQueue`

Release the shared command/response queue lock.

```cpp
static void unlockQueue(spin_lock_t* lock, uint32_t irqState)
```

### `allocateRequestId`

Allocate a non-zero request identifier for a Core 0 to Core 1 command.

```cpp
static uint32_t allocateRequestId()
```

### `pushCommand`

Push one command into the Core 1 command queue.

```cpp
static bool pushCommand(const CoreCommand& command)
```

### `popCommand`

Pop one pending command from the Core 1 command queue.

```cpp
static bool popCommand(CoreCommand& command)
```

### `pushResponse`

Push one command response into the Core 0 response queue.

```cpp
static bool pushResponse(const CoreResponse& response)
```

### `popResponse`

Find and remove the response matching a request identifier.

```cpp
static bool popResponse(CoreResponse& response)
```

### `fillResponse`

Build a CoreResponse structure from command execution data.

```cpp
static void fillResponse(CoreResponse& response,   const CoreCommand& command,   CoreResponseStatus status,   const char* message)
```

### `submitCoreCommandWait`

Submit a command to Core 1 and wait for the matching response with a timeout.

```cpp
bool submitCoreCommandWait(CoreCommand& command, CoreResponse& response, uint32_t timeoutMs)
```

### `requestSetChannelMask`

Request Core 1 to apply one 16-bit resistance mask to one channel.

```cpp
bool requestSetChannelMask(uint8_t channelIndex, uint16_t mask, char* reason, size_t reasonLen)
```

### `requestSetAllMasks`

Request Core 1 to apply eight channel masks after validating the full profile.

```cpp
bool requestSetAllMasks(const uint16_t masks[CHANNEL_COUNT], char* reason, size_t reasonLen)
```

### `requestAllOff`

Request Core 1 to switch every output channel OFF.

```cpp
bool requestAllOff(char* reason, size_t reasonLen)
```

### `applyChannelMask`

Core-0-safe wrapper that requests a per-channel mask change on Core 1.

```cpp
bool applyChannelMask(uint8_t channelIndex, uint16_t newMask)
```

### `applyAllMasksSafely`

Core-0-safe wrapper that requests an all-channel mask profile on Core 1.

```cpp
bool applyAllMasksSafely(const uint16_t masks[CHANNEL_COUNT], char* reason, size_t reasonLen)
```

### `forceAllOff`

Core-0-safe wrapper that requests an emergency/normal all-OFF transition.

```cpp
void forceAllOff()
```

### `processCoreCommand`

Execute one validated hardware command on Core 1.

```cpp
static void processCoreCommand(const CoreCommand& command)
```

### `core1ProcessEngineOnce`

Run one iteration of the Core 1 command-processing engine.

```cpp
void core1ProcessEngineOnce()
```

## `ethernet_startup.cpp`

### `startEthernetStatic`

Start W5500 Ethernet using the configured static IPv4 settings.

```cpp
bool startEthernetStatic()
```

## `http_handlers.cpp`

### `handlePing`

Handle Ping.

```cpp
void handlePing()
```

### `handleState`

Return the current machine state as a compact text/JSON-like response.

```cpp
void handleState()
```

### `handleLiveStatePage`

Handle Live State Page.

```cpp
void handleLiveStatePage()
```

### `handleRoot`

Handle Root.

```cpp
void handleRoot()
```

### `appendNetworkSettingsForm`

Append Network Settings Form.

```cpp
void appendNetworkSettingsForm(String& html)
```

### `handleNetwork`

Handle Network.

```cpp
void handleNetwork()
```

### `handleNetworkSave`

Handle Network Save.

```cpp
void handleNetworkSave()
```

### `handleRuntimePage`

Handle Runtime Page.

```cpp
void handleRuntimePage()
```

### `handleFilesPage`

Handle Files Page.

```cpp
void handleFilesPage()
```

### `handleSettings`

Handle Settings.

```cpp
void handleSettings()
```

### `handleUploadConfig`

Handle Upload Config.

```cpp
void handleUploadConfig()
```

### `handleUploadConfigApi`

Handle Upload Config Api.

```cpp
void handleUploadConfigApi()
```

### `handleDownloadConfig`

Handle Download Config.

```cpp
void handleDownloadConfig()
```

### `handleToggleBit`

Handle Toggle Bit.

```cpp
void handleToggleBit()
```

### `handleTargetApply`

Handle Target Apply.

```cpp
void handleTargetApply()
```

### `handleProfilesPage`

Handle Profiles Page.

```cpp
void handleProfilesPage()
```

### `handleProfileSave`

Handle Profile Save.

```cpp
void handleProfileSave()
```

### `handleProfileApply`

Handle Profile Apply.

```cpp
void handleProfileApply()
```

### `handleProfileDelete`

Handle Profile Delete.

```cpp
void handleProfileDelete()
```

### `handleSafetyPage`

Handle Safety Page.

```cpp
void handleSafetyPage()
```

### `handleSafetySave`

Handle Safety Save.

```cpp
void handleSafetySave()
```

### `handleScpiPage`

Render the browser-visible SCPI command reference page.

```cpp
void handleScpiPage()
```

### `handleLogPage`

Handle Log Page.

```cpp
void handleLogPage()
```

### `handleBackupPage`

Handle Backup Page.

```cpp
void handleBackupPage()
```

### `handleBackupDownload`

Handle Backup Download.

```cpp
void handleBackupDownload()
```

### `deleteFilesByPrefix`

Delete Files By Prefix.

```cpp
void deleteFilesByPrefix(const char* prefix)
```

### `handleFactoryReset`

Handle Factory Reset.

```cpp
void handleFactoryReset()
```

### `handleWatchdogPage`

Handle Watchdog Page.

```cpp
void handleWatchdogPage()
```

### `handleCalibrationMetaSave`

Handle Calibration Meta Save.

```cpp
void handleCalibrationMetaSave()
```

### `handleSet`

Handle Set.

```cpp
void handleSet()
```

### `handleAllOff`

Handle All Off.

```cpp
void handleAllOff()
```

### `handleNotFound`

Handle Not Found.

```cpp
void handleNotFound()
```

### `setupHttpServer`

Register all HTTP routes and start the web server.

```cpp
void setupHttpServer()
```

## `http_page_helpers.cpp`

### `appendCommonPageHeader`

Append Common Page Header.

```cpp
void appendCommonPageHeader(String& html, const char* title)
```

### `appendCommonPageFooter`

Append Common Page Footer.

```cpp
void appendCommonPageFooter(String& html)
```

## `littlefs_page_helpers.cpp`

### `u64ToString`

U64 To String.

```cpp
String u64ToString(uint64_t value)
```

### `formatBytesHuman`

Format Bytes Human.

```cpp
String formatBytesHuman(uint64_t bytes)
```

### `appendLittleFsChannelConfigStatus`

Append Little Fs Channel Config Status.

```cpp
void appendLittleFsChannelConfigStatus(String& html)
```

### `appendLittleFsStoredFiles`

Append Little Fs Stored Files.

```cpp
void appendLittleFsStoredFiles(String& html)
```

### `appendLittleFsStorageInfo`

Append Little Fs Storage Info.

```cpp
void appendLittleFsStorageInfo(String& html)
```

## `parse_helpers.cpp`

### `parseChannel`

Parse Channel.

```cpp
bool parseChannel(uint8_t& channelIndex)
```

### `parseBit`

Parse Bit.

```cpp
bool parseBit(uint8_t& bitIndex)
```

### `parseHex16String`

Parse a 16-bit mask from hexadecimal text with or without 0x prefix.

```cpp
bool parseHex16String(String s, uint16_t& mask)
```

### `parseMask`

Parse Mask.

```cpp
bool parseMask(uint16_t& mask)
```

## `resistance_calculation.cpp`

### `parseResistanceOhms`

Parse Resistance Ohms.

```cpp
bool parseResistanceOhms(const char* text, double& ohms)
```

### `formatResistanceOhms`

Format Resistance Ohms.

```cpp
String formatResistanceOhms(double ohms)
```

### `calculateOutputResistanceText`

Calculate Output Resistance Text.

```cpp
String calculateOutputResistanceText(uint8_t channelIndex, uint16_t mask)
```

### `calculateEquivalentOhms`

Calculate the equivalent resistance of all active branches in one channel mask.

```cpp
bool calculateEquivalentOhms(uint8_t channelIndex, uint16_t mask, double& outOhms)
```

### `countActiveBits16`

Count Active Bits16.

```cpp
uint8_t countActiveBits16(uint16_t mask)
```

### `checkMaskSafety`

Validate a mask against the configured minimum resistance and maximum active-bit limits.

```cpp
bool checkMaskSafety(uint8_t channelIndex, uint16_t mask, char* reason, size_t reasonLen)
```

### `resistanceCssClass`

Resistance Css Class.

```cpp
String resistanceCssClass(uint8_t channelIndex, uint16_t mask)
```

### `safetyConfigPath`

Safety Config Path.

```cpp
String safetyConfigPath()
```

### `calibrationMetaPath`

Calibration Meta Path.

```cpp
String calibrationMetaPath(uint8_t channelIndex)
```

### `appendLogEvent`

Append Log Event.

```cpp
void appendLogEvent(const char* text)
```

### `sanitizeName`

Sanitize Name.

```cpp
String sanitizeName(String name)
```

### `profilePathFromName`

Profile Path From Name.

```cpp
String profilePathFromName(String name)
```

### `saveSafetyConfigToLittleFS`

Save Safety Config To Little FS.

```cpp
bool saveSafetyConfigToLittleFS()
```

### `loadSafetyConfigFromLittleFS`

Load Safety Config From Little FS.

```cpp
bool loadSafetyConfigFromLittleFS()
```

### `findNearestMaskForTarget`

Search masks locally on the RP2040 to find the nearest output resistance.

```cpp
bool findNearestMaskForTarget(uint8_t channelIndex, double targetOhm, uint16_t& bestMask, double& bestOhm, double& bestErrorPercent)
```

### `profileListOptions`

Profile List Options.

```cpp
String profileListOptions()
```

### `loadProfileMasks`

Load Profile Masks.

```cpp
bool loadProfileMasks(String profileName, uint16_t masks[CHANNEL_COUNT])
```

### `saveCurrentProfile`

Save Current Profile.

```cpp
bool saveCurrentProfile(String profileName)
```

### `appendProfileManager`

Append Profile Manager.

```cpp
void appendProfileManager(String& html)
```

### `appendSafetySummaryCard`

Append Safety Summary Card.

```cpp
void appendSafetySummaryCard(String& html)
```

### `appendLiveStateScript`

Append Live State Script.

```cpp
void appendLiveStateScript(String& html)
```

### `appendBitIndicator`

Append one channel bit-status LED row to a generated HTML page.

```cpp
void appendBitIndicator(String& html, uint8_t channelIndex, uint16_t mask)
```

## `runtime_resistor_config.cpp`

### `copyDefaultConfigToRuntime`

Copy compile-time resistor branch definitions into the mutable runtime table.

```cpp
void copyDefaultConfigToRuntime()
```

### `getRuntimeResistorInfoForBit`

Get Runtime Resistor Info For Bit.

```cpp
RuntimeResistorInfo* getRuntimeResistorInfoForBit(uint8_t channelIndex, uint8_t bit)
```

### `channelConfigPath`

Channel Config Path.

```cpp
String channelConfigPath(uint8_t channelIndex)
```

### `networkConfigPath`

Network Config Path.

```cpp
String networkConfigPath()
```

### `channelConfigToText`

Channel Config To Text.

```cpp
String channelConfigToText(uint8_t channelIndex)
```

### `parseCsvConfigLine`

Parse Csv Config Line.

```cpp
bool parseCsvConfigLine(const String& line, RuntimeResistorInfo& out)
```

### `parseHeaderInitializerLine`

Parse Header Initializer Line.

```cpp
bool parseHeaderInitializerLine(const String& line, RuntimeResistorInfo& out)
```

### `parseChannelConfigText`

Parse Channel Config Text.

```cpp
bool parseChannelConfigText(uint8_t channelIndex, const String& text, char* error, size_t errorLen)
```

### `saveChannelConfigToLittleFS`

Save Channel Config To Little FS.

```cpp
bool saveChannelConfigToLittleFS(uint8_t channelIndex)
```

### `loadChannelConfigFromLittleFS`

Load Channel Config From Little FS.

```cpp
bool loadChannelConfigFromLittleFS(uint8_t channelIndex)
```

### `loadAllRuntimeConfigs`

Load all per-channel runtime resistor configuration files from LittleFS.

```cpp
void loadAllRuntimeConfigs()
```

## `scpi_server.cpp`

### `scpiPrintHelp`

Print the supported SCPI command list to an active TCP client.

```cpp
void scpiPrintHelp(WiFiClient& client)
```

### `parseScpiChannelCommand`

Parse SCPI channel-command aliases and return a zero-based channel index.

```cpp
bool parseScpiChannelCommand(String cmd, uint8_t& channelIndex, String& rest)
```

### `scpiPrintOhmsFromConfig`

Print one resistor value from the runtime calibration table.

```cpp
static void scpiPrintOhmsFromConfig(WiFiClient& client, const char* resistanceText)
```

### `scpiPrintCalibrationChannelCompact`

Print one channel calibration table in compact SCPI response format.

```cpp
static void scpiPrintCalibrationChannelCompact(WiFiClient& client, uint8_t channelIndex)
```

### `scpiPrintCalibrationAllCompact`

Print all channel calibration tables for PC-side nearest-mask calculation.

```cpp
static void scpiPrintCalibrationAllCompact(WiFiClient& client)
```

### `parseOptionalCalibrationChannelArgument`

Parse an optional channel selector after a calibration query command.

```cpp
static bool parseOptionalCalibrationChannelArgument(const String& commandTail, uint8_t& channelIndex)
```

### `tryHandleCalibrationQuery`

Handle CAL:RES and calibration-table SCPI query aliases.

```cpp
static bool tryHandleCalibrationQuery(String cmd, WiFiClient& client)
```

### `processScpiLine`

Parse and execute one received SCPI command line.

```cpp
void processScpiLine(const char* rawLine, WiFiClient& client)
```

### `setupScpiServer`

Start the raw TCP SCPI server.

```cpp
void setupScpiServer()
```

### `handleScpiServer`

Accept SCPI clients and process line-oriented commands.

```cpp
void handleScpiServer()
```

## `setup_loop.cpp`

### `setup`

Arduino Core 0 startup entry point for communications and non-real-time services.

```cpp
void setup()
```

### `loop`

Arduino Core 0 loop for HTTP, SCPI, heartbeat, and monitoring tasks.

```cpp
void loop()
```

### `setup1`

Arduino Core 1 startup entry point for deterministic output control.

```cpp
void setup1()
```

### `loop1`

Arduino Core 1 loop that runs the hardware command engine.

```cpp
void loop1()
```

## `shift_registers.cpp`

### `pulseClock`

Generate one shift-register clock pulse using the configured timing.

```cpp
void pulseClock()
```

### `pulseLatch`

Pulse exactly one channel latch pin.

```cpp
void pulseLatch(uint8_t channelIndex)
```

### `pulseShiftRegisterClear`

Pulse SRCLR low to clear the cascaded 74HC595 outputs.

```cpp
void pulseShiftRegisterClear()
```

### `shiftMaskBit0First`

Shift a 16-bit mask to the register chain with bit 0 transmitted first.

```cpp
void shiftMaskBit0First(uint16_t mask)
```

### `latchMaskToChannel`

Shift and latch a mask into exactly one channel output register.

```cpp
bool latchMaskToChannel(uint8_t channelIndex, uint16_t mask)
```

### `applyChannelMaskPhysical`

Apply one channel mask with break-before-make sequencing on Core 1.

```cpp
bool applyChannelMaskPhysical(uint8_t channelIndex, uint16_t newMask)
```

### `applyAllMasksSafelyPhysical`

Validate and apply an eight-channel mask set using physical Core 1 switching.

```cpp
bool applyAllMasksSafelyPhysical(const uint16_t masks[CHANNEL_COUNT], char* reason, size_t reasonLen)
```

### `setupShiftRegisters`

Configure shift-register GPIO pins, pulse SRCLR, and force known-safe outputs.

```cpp
void setupShiftRegisters()
```

### `forceAllOffPhysical`

Physically latch 0x0000 into all eight output channels.

```cpp
void forceAllOffPhysical()
```

## `status_led.cpp`

### `ws2812Set`

Ws2812 Set.

```cpp
void ws2812Set(uint8_t r, uint8_t g, uint8_t b)
```

### `heartbeatBegin`

Initialize the WS2812 heartbeat pixel.

```cpp
void heartbeatBegin()
```

### `setLedMode`

Set Led Mode.

```cpp
void setLedMode(LedMode mode)
```

### `updateHeartbeat`

Update the WS2812 indicator according to the current LED mode.

```cpp
void updateHeartbeat()
```

## `utility.cpp`

### `ipToString`

Ip To String.

```cpp
String ipToString(const IPAddress& ip)
```

### `makeBoardSerialNumber`

Make Board Serial Number.

```cpp
String makeBoardSerialNumber()
```

### `parseIpAddressText`

Parse Ip Address Text.

```cpp
bool parseIpAddressText(const String& text, IPAddress& out)
```

### `networkConfigToText`

Network Config To Text.

```cpp
String networkConfigToText()
```

### `saveNetworkConfigToLittleFS`

Save Network Config To Little FS.

```cpp
bool saveNetworkConfigToLittleFS()
```

### `loadNetworkConfigFromLittleFS`

Load Network Config From Little FS.

```cpp
bool loadNetworkConfigFromLittleFS()
```

### `hex16`

Hex16.

```cpp
String hex16(uint16_t value)
```

### `setStatus`

Set Status.

```cpp
void setStatus(const char* text)
```

### `setLastError`

Set Last Error.

```cpp
void setLastError(const char* text)
```

### `clearLastError`

Clear Last Error.

```cpp
void clearLastError()
```

### `sendNoCacheHeaders`

Send No Cache Headers.

```cpp
void sendNoCacheHeaders()
```

### `redirectToRoot`

Redirect To Root.

```cpp
void redirectToRoot()
```

### `redirectToSettings`

Redirect To Settings.

```cpp
void redirectToSettings()
```

### `safeState`

Enter a firmware safe state and request all outputs OFF.

```cpp
void safeState(const char* reason)
```

### `noteHttpRequest`

Note Http Request.

```cpp
void noteHttpRequest()
```

### `noteScpiCommand`

Note Scpi Command.

```cpp
void noteScpiCommand()
```

### `runtimeMonitorBegin`

Runtime Monitor Begin.

```cpp
void runtimeMonitorBegin()
```

### `runtimeMonitorUpdate`

Runtime Monitor Update.

```cpp
void runtimeMonitorUpdate(uint32_t busyUs)
```

### `getHeapTotalBytes`

Get Heap Total Bytes.

```cpp
uint32_t getHeapTotalBytes()
```

### `getHeapUsedBytes`

Get Heap Used Bytes.

```cpp
uint32_t getHeapUsedBytes()
```

### `getHeapFreeBytes`

Get Heap Free Bytes.

```cpp
uint32_t getHeapFreeBytes()
```

### `getHeapUsedPercent`

Get Heap Used Percent.

```cpp
float getHeapUsedPercent()
```

### `appendRuntimeMonitorInfo`

Append Runtime Monitor Info.

```cpp
void appendRuntimeMonitorInfo(String& html)
```

## `w5500_registers.cpp`

### `w5500WriteCommonReg`

Write one W5500 common-register byte through SPI.

```cpp
void w5500WriteCommonReg(uint16_t address, uint8_t value, uint32_t spiHz)
```

### `w5500ReadCommonReg`

Read one W5500 common-register byte through SPI.

```cpp
uint8_t w5500ReadCommonReg(uint16_t address, uint32_t spiHz)
```

### `w5500SoftwareResetAndProbe`

Issue W5500 software reset and verify VERSIONR communication.

```cpp
bool w5500SoftwareResetAndProbe()
```
