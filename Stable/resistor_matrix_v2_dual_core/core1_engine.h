/**
 * @file core1_engine.h
 * @brief Minimal Core 1 interface with no UI, storage, protocol parser, or String dependency.
 */
#pragma once

#include <Arduino.h>
#include <pico/sync.h>
#include "hardware_config.h"
#include "core_transport_types.h"

#ifndef ERESISTOR_LOG_LEVEL
#define ERESISTOR_LOG_LEVEL 1
#endif

bool coreTransportIsInitialized();
bool coreTransportWaitUntilInitialized(uint32_t timeoutMs);
bool coreTransportPopCommand(CoreCommand& command);
bool coreTransportPushResult(const CoreResult& result);
uint32_t coreTransportCurrentGeneration();
uint32_t coreTransportInvalidateGeneration();
void coreTransportPublishSnapshot(const CoreOutputSnapshot& snapshot);
bool coreTransportReadPolicySnapshot(uint8_t slot, uint32_t expectedGeneration, CoreSafetySnapshot& snapshot);
void coreTransportSignalCore1Ready();
void coreTransportSetCore1StartupStage(uint32_t stage);
uint32_t coreTransportGetCore1StartupStage();
uint32_t coreTransportGetCore1ReadyToken();
bool coreTransportCore0HeartbeatExpired(uint32_t nowMs);
void coreTransportRecordExpired();
void coreTransportRecordGenerationReject();
void coreTransportRecordInvalidCommand();
void coreTransportRecordPolicyInstall();
void coreTransportRecordCore0Failsafe();
void coreTransportRecordProfileResult(uint32_t durationUs, uint32_t clearDurationUs,
                                      uint8_t breakBeforeMakeOperations, bool success);

bool initCore1EventQueue();
bool core1EmitEvent(Core1EventCode code, FirmwareLogLevel level, uint8_t channelIndex,
                    uint16_t mask, uint16_t detail, uint32_t durationUs, uint32_t sequence = 0);

bool core1PhysicalInitialize();
bool core1ApplyChannelMaskPhysical(uint8_t channelIndex, uint16_t newMask, uint32_t sequence);
bool core1ApplyAllMasksPhysical(const uint16_t masks[CHANNEL_COUNT], uint32_t sequence);
bool core1ForceAllOffPhysical(uint32_t sequence, uint16_t detail = CORE_DETAIL_NONE);
const uint16_t* core1PhysicalMasks();
const uint32_t* core1PhysicalApplyCounters();
bool core1PhysicalOutputsSafe();
bool core1PhysicalReady();

void core1ProcessEngineOnce();
void setup1();
void loop1();

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

#ifdef ERESISTOR_TEST_MODE
void core1TestSetNextCommandDelayMs(uint32_t delayMs);
void core1TestPauseProcessingMs(uint32_t pauseMs);
void core1TestInvalidateNextCommandGeneration();
void core1TestFailNextProfileAfterClear();
#endif
