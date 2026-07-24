/** @file firmware_identity.h @brief Immutable build identity and factory defaults. */
#pragma once
#include "platform.h"

inline constexpr uint8_t DEFAULT_DEVICE_IP_OCTETS[4] = {192, 168, 0, 55};
inline constexpr uint8_t DEFAULT_DEVICE_DNS_OCTETS[4] = {0, 0, 0, 0};
inline constexpr uint8_t DEFAULT_DEVICE_GATEWAY_OCTETS[4] = {0, 0, 0, 0};
inline constexpr uint8_t DEFAULT_DEVICE_SUBNET_OCTETS[4] = {255, 255, 255, 0};
inline constexpr const char* DEFAULT_DEVICE_IP_TEXT = "192.168.0.55";

#ifndef ERESISTOR_LOG_LEVEL
#define ERESISTOR_LOG_LEVEL 1
#endif

inline constexpr const char* FIRMWARE_NAME = "E-Resistor";
inline constexpr const char* FIRMWARE_VENDOR = "OpenBench";
inline constexpr const char* FIRMWARE_VERSION = "0.8.0";
inline constexpr const char* FIRMWARE_BUILD_DATE = __DATE__;
inline constexpr const char* FIRMWARE_BUILD_TIME = __TIME__;
inline constexpr const char* API_VERSION = "v1";
inline constexpr uint32_t STATIC_ASSET_MAX_AGE_SECONDS = 86400UL;
inline constexpr uint32_t GATE4_CALIBRATION_TEMP_HEAP_BASELINE_BYTES = 24576UL;
