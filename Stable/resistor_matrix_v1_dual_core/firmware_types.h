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

enum LedMode : uint8_t {
  LED_BOOT,
  LED_OK,
  LED_ACTIVITY,
  LED_FAULT,
  LED_IDENTIFY_BLUE
};
