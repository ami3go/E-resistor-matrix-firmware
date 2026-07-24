/** @file core_api.h @brief LED, W5500 and deterministic dual-core command interfaces. */
#pragma once
#include "runtime_state.h"
void initCoreCommandEngine();
bool initCore1EventQueue();
bool core1EmitEvent(Core1EventCode code, FirmwareLogLevel level, uint8_t channelIndex,
                    uint16_t mask, uint16_t detail, uint32_t durationUs, uint32_t sequence = 0);
bool popCore1Event(Core1Event& event);
void drainCore1Events();
const char* core1EventCodeText(Core1EventCode code);
void ws2812Set(uint8_t r, uint8_t g, uint8_t b);
void heartbeatBegin();
void setLedMode(LedMode mode);
void startIdentifyLedBlink(uint32_t durationMs);
void updateHeartbeat();
void w5500WriteCommonReg(uint16_t address, uint8_t value, uint32_t spiHz);
uint8_t w5500ReadCommonReg(uint16_t address, uint32_t spiHz);
bool w5500SoftwareResetAndProbe();
bool waitForCore1Startup(uint32_t timeoutMs = CORE_STARTUP_TIMEOUT_MS);
bool refreshCore0OutputMirror();
bool readCoreOutputSnapshot(CoreOutputSnapshot& snapshot);
bool submitCoreCommandWait(CoreCommand& command, CoreResult& result, uint32_t timeoutMs = CORE_COMMAND_TIMEOUT_MS);
bool requestSetChannelMask(uint8_t channelIndex, uint16_t mask, char* reason, size_t reasonLen);
bool requestSetAllMasks(const uint16_t masks[CHANNEL_COUNT], char* reason, size_t reasonLen);
bool requestAllOff(char* reason, size_t reasonLen);
bool installCore1PolicySnapshot(char* reason = nullptr, size_t reasonLen = 0);
void getCoreTransportDiagnostics(CoreTransportDiagnostics& diagnostics);
void coreTransportKickCore0Heartbeat();
uint32_t coreTransportInvalidateGeneration();
uint32_t coreTransportGetCore1StartupStage();
uint32_t coreTransportGetCore1ReadyToken();
bool applyChannelMask(uint8_t channelIndex, uint16_t newMask);
bool applyAllMasksSafely(const uint16_t masks[CHANNEL_COUNT], char* reason, size_t reasonLen);
bool forceAllOff(char* reason = nullptr, size_t reasonLen = 0);
#ifdef ERESISTOR_TEST_MODE
void core1TestSetNextCommandDelayMs(uint32_t delayMs);
void core1TestPauseProcessingMs(uint32_t pauseMs);
void core1TestInvalidateNextCommandGeneration();
void core1TestFailNextProfileAfterClear();
#endif
bool startEthernetStatic();
