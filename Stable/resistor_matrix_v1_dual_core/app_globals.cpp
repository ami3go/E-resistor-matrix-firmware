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

IPAddress DEVICE_IP(
  DEFAULT_DEVICE_IP_OCTETS[0], DEFAULT_DEVICE_IP_OCTETS[1],
  DEFAULT_DEVICE_IP_OCTETS[2], DEFAULT_DEVICE_IP_OCTETS[3]
);
IPAddress DEVICE_DNS(
  DEFAULT_DEVICE_DNS_OCTETS[0], DEFAULT_DEVICE_DNS_OCTETS[1],
  DEFAULT_DEVICE_DNS_OCTETS[2], DEFAULT_DEVICE_DNS_OCTETS[3]
);
IPAddress DEVICE_GATEWAY(
  DEFAULT_DEVICE_GATEWAY_OCTETS[0], DEFAULT_DEVICE_GATEWAY_OCTETS[1],
  DEFAULT_DEVICE_GATEWAY_OCTETS[2], DEFAULT_DEVICE_GATEWAY_OCTETS[3]
);
IPAddress DEVICE_SUBNET(
  DEFAULT_DEVICE_SUBNET_OCTETS[0], DEFAULT_DEVICE_SUBNET_OCTETS[1],
  DEFAULT_DEVICE_SUBNET_OCTETS[2], DEFAULT_DEVICE_SUBNET_OCTETS[3]
);

Wiznet5500lwIP eth(ETH_CS);
WebServer server(HTTP_TCP_PORT);
WiFiServer scpiServer(SCPI_TCP_PORT);
WiFiClient scpiClient;

char scpiLine[160] = {0};
size_t scpiLineLen = 0;
bool scpiDiscardUntilNewline = false;

float channelResistorOhms[CHANNEL_COUNT][BIT_COUNT] = {};

bool ethernetFault = false;
bool ethernetInterfaceStarted = false;
bool ethernetServicesStarted = false;
bool ethernetLinkUp = false;
bool ethernetNetworkErrorActive = false;
EthernetRecoveryState ethernetRecoveryState = ETHERNET_RECOVERY_UNINITIALIZED;
uint32_t ethernetRecoveryAttemptCount = 0;
uint32_t ethernetRecoverySuccessCount = 0;
uint32_t ethernetLinkDownCount = 0;
uint32_t ethernetLastProbeMs = 0;
uint32_t ethernetLastRecoveryAttemptMs = 0;
uint32_t ethernetLastTransitionMs = 0;
bool littleFsReady = false;
uint8_t calibrationSavedMask = 0;
uint8_t calibrationLoadedMask = 0;
uint8_t calibrationLoadErrorMask = 0;
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

double safetyMinOhm[CHANNEL_COUNT] = {300.0, 300.0, 300.0, 300.0, 300.0, 300.0, 300.0, 300.0};
double safetyMaxOhm[CHANNEL_COUNT] = {
  20000000.0, 20000000.0, 20000000.0, 20000000.0,
  20000000.0, 20000000.0, 20000000.0, 20000000.0
};
uint8_t safetyMaxActiveBits[CHANNEL_COUNT] = {16, 16, 16, 16, 16, 16, 16, 16};
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
volatile uint32_t core1LoopMaxUs = 0;
volatile uint32_t core1MinFreeStackBytes = UINT32_MAX;
volatile uint32_t core1EventCounter = 0;
volatile uint32_t core1EventDropCounter = 0;

uint32_t targetSearchLastCandidates = 0;
uint32_t targetSearchLastElapsedUs = 0;
uint32_t targetSearchTimeoutCount = 0;
uint32_t targetSearchCancelCount = 0;

// Arduino-Pico: allocate an independent 8 KiB Core 1 stack instead of
// splitting the default 8 KiB stack into two 4 KiB regions.
bool core1_separate_stack = true;

// ============================================================
// Gate 5 HTTP streaming and API telemetry
// ============================================================
uint32_t httpStreamedResponseCount = 0;
uint32_t httpCalibrationPageLastTempBytes = 0;
uint32_t httpCalibrationPagePeakTempBytes = 0;
uint32_t httpStateLastTempBytes = 0;
uint32_t httpStatePeakTempBytes = 0;
uint32_t httpMethodRejectedCount = 0;
uint32_t httpApiV1RequestCount = 0;
