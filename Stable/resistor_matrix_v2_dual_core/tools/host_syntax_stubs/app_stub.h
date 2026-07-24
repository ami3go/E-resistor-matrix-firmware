#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include "Arduino.h"
#include "core_transport_types.h"

extern uint16_t channelMask[CHANNEL_COUNT];
extern uint32_t applyCounter[CHANNEL_COUNT];
extern bool outputsKnownSafe;
extern bool shiftRegistersReady;
extern volatile bool core1EngineReady;
extern volatile bool emergencyOffRequested;
extern volatile uint32_t core1QueueOverflowCounter;
extern float channelResistorOhms[CHANNEL_COUNT][BIT_COUNT];
extern double safetyMinOhm[CHANNEL_COUNT];
extern double safetyMaxOhm[CHANNEL_COUNT];
extern uint8_t safetyMaxActiveBits[CHANNEL_COUNT];
extern bool safetyExpertMode;
extern bool fatalSafeStateActive;
enum LedMode : uint8_t { LED_BOOT, LED_OK, LED_ACTIVITY, LED_FAULT, LED_IDENTIFY_BLUE };
bool initCore1EventQueue();
bool checkMaskSafety(uint8_t, uint16_t, char*, size_t);
void updateHeartbeat();
void setLastError(const char*);
void setLedMode(LedMode);
void setStatus(const char*);
bool refreshCore0OutputMirror();
