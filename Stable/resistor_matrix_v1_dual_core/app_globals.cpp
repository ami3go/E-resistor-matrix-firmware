/**
 * @file app_globals.cpp
 * @brief Global object and state definitions shared between the Arduino modules.
 */

#include "app.h"

Adafruit_NeoPixel heartbeatPixel(
  WS2812_COUNT,
  WS2812_PIN,
  NEO_GRB + NEO_KHZ800
);

LedMode ledMode = LED_BOOT;
LedMode ledModeBeforeIdentify = LED_BOOT;
bool ledPhase = false;
uint32_t lastLedUpdateMs = 0;
uint32_t ledIdentifyUntilMs = 0;
uint8_t ledIdentifyPatternStep = 0;

IPAddress DEVICE_IP(192, 168, 7, 50);
IPAddress DEVICE_DNS(0, 0, 0, 0);
IPAddress DEVICE_GATEWAY(0, 0, 0, 0);
IPAddress DEVICE_SUBNET(255, 255, 255, 0);

Wiznet5500lwIP eth(ETH_CS);
WebServer server(HTTP_TCP_PORT);
WiFiServer scpiServer(SCPI_TCP_PORT);
WiFiClient scpiClient;

char scpiLine[160] = {0};
size_t scpiLineLen = 0;

RuntimeResistorInfo channelResistorTable[CHANNEL_COUNT][BIT_COUNT] = {};

bool ethernetFault = false;
bool littleFsReady = false;
uint8_t w5500Version = 0;

uint16_t channelMask[CHANNEL_COUNT] = {
  0, 0, 0, 0, 0, 0, 0, 0
};

uint32_t applyCounter[CHANNEL_COUNT] = {
  0, 0, 0, 0, 0, 0, 0, 0
};

bool shiftRegistersReady = false;
bool outputsKnownSafe = false;
bool fatalSafeStateActive = false;

char statusText[128] = "Boot";
char lastError[128] = "0,\"No error\"";
String deviceSerialNumber = "UNKNOWN";

uint32_t runtimeWindowStartMs = 0;
uint64_t runtimeBusyAccumUs = 0;
uint32_t runtimeLoopCountWindow = 0;
uint32_t runtimeLoopMaxUs = 0;
float runtimeCore0LoadPct = 0.0f;
float runtimeLoopsPerSecond = 0.0f;
uint32_t httpRequestCount = 0;
uint32_t scpiCommandCount = 0;
uint32_t bootMillis = 0;

double safetyMinOhm = 300.0;
double safetyMaxOhm = 20000000.0;
uint8_t safetyMaxActiveBits = 16;
bool safetyExpertMode = false;

char eventLog[32][128] = {{0}};
uint8_t eventLogCount = 0;
uint8_t eventLogHead = 0;
char lastScpiCommand[96] = "";
bool firmwareUpdateInProgress = false;
bool firmwareUpdateSucceeded = false;
char firmwareUpdateStatus[160] = "No firmware update has been attempted";

// ============================================================
// Dual-core hardware engine state
// ============================================================
volatile bool core1EngineReady = false;
volatile bool core1OutputsReady = false;
volatile bool core1Fault = false;
volatile bool emergencyOffRequested = false;
volatile uint32_t core1HeartbeatMs = 0;
volatile uint32_t core1LoopCounter = 0;
volatile uint32_t core1CommandCounter = 0;
volatile uint32_t core1QueueOverflowCounter = 0;
volatile uint32_t core1LastCommandMs = 0;
