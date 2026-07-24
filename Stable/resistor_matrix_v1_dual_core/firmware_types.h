/** @file firmware_types.h @brief Shared value types without service ownership. */
#pragma once
#include "platform.h"

struct ParsedResistorInfo {
  uint8_t bit;
  float resistanceOhm;
};

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

enum EthernetRecoveryState : uint8_t {
  ETHERNET_RECOVERY_UNINITIALIZED = 0,
  ETHERNET_RECOVERY_HARDWARE_RETRY,
  ETHERNET_RECOVERY_WAIT_LINK,
  ETHERNET_RECOVERY_ONLINE
};

enum LedMode : uint8_t {
  LED_BOOT,
  LED_OK,
  LED_ACTIVITY,
  LED_FAULT,
  LED_IDENTIFY_BLUE
};
