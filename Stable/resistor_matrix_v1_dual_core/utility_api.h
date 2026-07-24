/** @file utility_api.h @brief Utility, monitor, network-configuration and parsing interfaces. */
#pragma once
#include "runtime_state.h"
String ipToString(const IPAddress& ip);
String makeBoardSerialNumber();
bool parseIpAddressText(const String& text, IPAddress& out);
String networkConfigToText();
bool saveNetworkConfigToLittleFS();
bool loadNetworkConfigFromLittleFS();
String hex16(uint16_t value);
void setStatus(const char* text);
void setLastError(const char* text);
void clearLastError();
void sendNoCacheHeaders();
void redirectToRoot();
void redirectToSettings();
void safeState(const char* reason);
void noteHttpRequest();
void noteScpiCommand();
void runtimeMonitorBegin();
void runtimeMonitorUpdate(uint32_t busyUs);
uint32_t getHeapTotalBytes();
uint32_t getHeapUsedBytes();
uint32_t getHeapFreeBytes();
float getHeapUsedPercent();
void appendRuntimeMonitorInfo(String& html);
String firmwareVersionString();
String firmwareIdentityString();
bool parseChannel(uint8_t& channelIndex);
bool parseBit(uint8_t& bitIndex);
bool parseHex16String(String s, uint16_t& mask);
bool parseMask(uint16_t& mask);
