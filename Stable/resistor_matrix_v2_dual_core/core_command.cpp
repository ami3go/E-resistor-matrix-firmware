/**
 * @file core_command.cpp
 * @brief Core 0 command submission, policy installation, and human-readable error mapping.
 */
#include "app.h"

void coreTransportInitialize();
bool coreTransportPushCommand(const CoreCommand& command);
bool coreTransportWaitForResult(CoreResult& result, uint32_t timeoutMs);
bool coreTransportWaitForCore1Ready(uint32_t timeoutMs);
uint32_t coreTransportGetCore1StartupStage();
uint32_t coreTransportGetCore1ReadyToken();
uint32_t coreTransportAllocateSequence();
uint32_t coreTransportCurrentGeneration();
uint32_t coreTransportInvalidateGeneration();
bool coreTransportReadSnapshot(CoreOutputSnapshot& snapshot);
bool coreTransportStagePolicySnapshot(const CoreSafetySnapshot& input, uint8_t& slot, uint32_t& generation);
void coreTransportKickCore0Heartbeat();
void coreTransportGetDiagnostics(CoreTransportDiagnostics& out);
void coreTransportRecordTimeout();

static bool allMasksZero(const CoreOutputSnapshot& snapshot) {
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) if (snapshot.masks[ch] != 0U) return false;
  return true;
}

void initCoreCommandEngine() {
  // The event queue must be initialized exactly once by Core 0 before the
  // transport-ready flag can wake Core 1. This removes a dual-core race where
  // both cores could attempt to claim and install the event-queue spin lock.
  initCore1EventQueue();
  coreTransportInitialize();
  coreTransportKickCore0Heartbeat();
}

bool waitForCore1Startup(uint32_t timeoutMs) {
  const uint32_t startMs = millis();
  bool readyTokenSeen = false;

  // Poll both the persistent ready token and the coherent snapshot. This is
  // level-triggered and therefore cannot lose an early Core 1 notification.
  while ((millis() - startMs) < timeoutMs) {
    readyTokenSeen = coreTransportGetCore1ReadyToken() == CORE1_READY_TOKEN;
    if (refreshCore0OutputMirror()) {
      if (shiftRegistersReady && outputsKnownSafe) return true;
      const uint32_t stage = coreTransportGetCore1StartupStage();
      if (readyTokenSeen && (stage == CORE1_STARTUP_GPIO_FAILED ||
                             stage == CORE1_STARTUP_ALL_OFF_FAILED)) {
        return false;
      }
    }
    delay(1);
  }

  // One final read covers a readiness publication concurrent with timeout.
  refreshCore0OutputMirror();
  return shiftRegistersReady && outputsKnownSafe;
}

static void updateCore0MirrorFromSnapshot(const CoreOutputSnapshot& snapshot) {
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
    channelMask[ch] = snapshot.masks[ch];
    applyCounter[ch] = snapshot.applyCounter[ch];
  }
  outputsKnownSafe = (snapshot.flags & CORE_SNAPSHOT_OUTPUTS_SAFE) != 0U;
  shiftRegistersReady = (snapshot.flags & CORE_SNAPSHOT_ENGINE_READY) != 0U;
  // Core 1 is the sole writer of core1EngineReady/core1OutputsReady/core1Fault.
  // Core 0 consumes the atomic snapshot through its own mirror variables only.
}

bool refreshCore0OutputMirror() {
  CoreOutputSnapshot snapshot{};
  if (!coreTransportReadSnapshot(snapshot)) return false;
  updateCore0MirrorFromSnapshot(snapshot);
  return true;
}

bool readCoreOutputSnapshot(CoreOutputSnapshot& snapshot) {
  if (!coreTransportReadSnapshot(snapshot)) return false;
  updateCore0MirrorFromSnapshot(snapshot);
  return true;
}

static const char* responseStatusText(CoreResponseStatus status, uint16_t detail) {
  (void)detail;
  switch (status) {
    case CORE_RESP_OK: return "OK";
    case CORE_RESP_BUSY: return "Core command queue full";
    case CORE_RESP_TIMEOUT: return "Core 1 command timeout";
    case CORE_RESP_EXPIRED: return "Core 1 rejected expired command";
    case CORE_RESP_GENERATION_MISMATCH: return "Core 1 rejected invalidated command generation";
    case CORE_RESP_ENGINE_NOT_READY: return "Core 1 hardware engine not ready";
    case CORE_RESP_POLICY_NOT_INSTALLED: return "Core 1 safety policy not installed";
    case CORE_RESP_INVALID_ARGUMENT: return "Invalid Core 1 command argument";
    case CORE_RESP_REJECTED: return "Core 1 safety policy rejected command";
    case CORE_RESP_IO_ERROR: return "Core 1 physical output operation failed";
    default: return "Unknown Core 1 status";
  }
}

bool submitCoreCommandWait(CoreCommand& command, CoreResult& result, uint32_t timeoutMs) {
  memset(&result, 0, sizeof(result));
  coreTransportKickCore0Heartbeat();
  refreshCore0OutputMirror();
  if (!core1EngineReady) {
    result.status = CORE_RESP_ENGINE_NOT_READY;
    return false;
  }

  command.sequence = coreTransportAllocateSequence();
  command.safetyGeneration = coreTransportCurrentGeneration();
  const uint64_t timeoutUs64 = uint64_t(timeoutMs) * 1000ULL;
  const uint32_t boundedUs = timeoutUs64 > 0x7FFFFFFFULL ? 0x7FFFFFFFUL : uint32_t(timeoutUs64);
  command.deadlineAtUs = micros() + boundedUs;

  if (!coreTransportPushCommand(command)) {
    result.sequence = command.sequence;
    result.status = CORE_RESP_BUSY;
    core1QueueOverflowCounter++;
    return false;
  }

  const uint32_t startMs = millis();
  while ((millis() - startMs) < timeoutMs) {
    const uint32_t elapsed = millis() - startMs;
    const uint32_t remaining = timeoutMs > elapsed ? timeoutMs - elapsed : 0U;
    CoreResult candidate{};
    if (!coreTransportWaitForResult(candidate, remaining)) break;
    if (candidate.sequence == command.sequence) {
      result = candidate;
      refreshCore0OutputMirror();
      return result.status == CORE_RESP_OK;
    }
  }

  result.sequence = command.sequence;
  result.status = CORE_RESP_TIMEOUT;
  coreTransportRecordTimeout();
  coreTransportInvalidateGeneration();
  emergencyOffRequested = true;
  return false;
}

static bool validateChannelMaskCore0(uint8_t channelIndex, uint16_t mask, char* reason, size_t reasonLen) {
  if (!checkMaskSafety(channelIndex, mask, reason, reasonLen)) return false;
  return true;
}

bool requestSetChannelMask(uint8_t channelIndex, uint16_t mask, char* reason, size_t reasonLen) {
  if (reason != nullptr && reasonLen > 0U) reason[0] = '\0';
  if (!validateChannelMaskCore0(channelIndex, mask, reason, reasonLen)) return false;
  CoreCommand command{};
  command.type = CORE_CMD_SET_MASK;
  command.channelIndex = channelIndex;
  command.mask = mask;
  CoreResult result{};
  const bool ok = submitCoreCommandWait(command, result, CORE_COMMAND_TIMEOUT_MS);
  if (!ok && reason != nullptr && reasonLen > 0U) snprintf(reason, reasonLen, "%s (detail=%u)", responseStatusText(result.status, result.detail), unsigned(result.detail));
  return ok;
}

bool requestSetAllMasks(const uint16_t masks[CHANNEL_COUNT], char* reason, size_t reasonLen) {
  if (reason != nullptr && reasonLen > 0U) reason[0] = '\0';
  if (masks == nullptr) {
    if (reason != nullptr && reasonLen > 0U) snprintf(reason, reasonLen, "missing mask array");
    return false;
  }
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) if (!validateChannelMaskCore0(ch, masks[ch], reason, reasonLen)) return false;
  CoreCommand command{};
  command.type = CORE_CMD_SET_ALL_MASKS;
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) command.masks[ch] = masks[ch];
  CoreResult result{};
  const bool ok = submitCoreCommandWait(command, result, CORE_COMMAND_TIMEOUT_MS * 2U);
  if (!ok && reason != nullptr && reasonLen > 0U) snprintf(reason, reasonLen, "%s (detail=%u)", responseStatusText(result.status, result.detail), unsigned(result.detail));
  return ok;
}

static bool waitForDirectAllOff(char* reason, size_t reasonLen) {
  const uint32_t startMs = millis();
  while ((millis() - startMs) < CORE_COMMAND_TIMEOUT_MS) {
    coreTransportKickCore0Heartbeat();
    refreshCore0OutputMirror();
    if (!emergencyOffRequested && outputsKnownSafe) {
      bool allZero = true;
      for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
        if (channelMask[ch] != 0U) { allZero = false; break; }
      }
      if (allZero) return true;
    }
    updateHeartbeat();
    delay(1);
  }
  if (reason != nullptr && reasonLen > 0U) snprintf(reason, reasonLen, "Core 1 direct all-off timeout");
  return false;
}

bool requestAllOff(char* reason, size_t reasonLen) {
  if (reason != nullptr && reasonLen > 0U) reason[0] = '\0';

  // Normal ALL:OFF is a sequenced command and does not invalidate the active
  // safety policy. Generation invalidation is reserved for timeout/fault paths.
  CoreCommand command{};
  command.type = CORE_CMD_CLEAR_ALL;
  CoreResult result{};
  if (submitCoreCommandWait(command, result, CORE_COMMAND_TIMEOUT_MS)) {
    refreshCore0OutputMirror();
    for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
      if (channelMask[ch] != 0U) {
        if (reason != nullptr && reasonLen > 0U) snprintf(reason, reasonLen, "Core 1 all-off snapshot verification failed");
        return false;
      }
    }
    return outputsKnownSafe;
  }

  // A timeout already invalidates the generation and raises emergencyOffRequested.
  // Other command failures take the same conservative direct-all-off fallback.
  if (!emergencyOffRequested) {
    coreTransportInvalidateGeneration();
    emergencyOffRequested = true;
  }
  if (waitForDirectAllOff(reason, reasonLen)) return true;
  if (reason != nullptr && reasonLen > 0U && reason[0] == '\0') {
    snprintf(reason, reasonLen, "%s (detail=%u)", responseStatusText(result.status, result.detail), unsigned(result.detail));
  }
  return false;
}

bool installCore1PolicySnapshot(char* reason, size_t reasonLen) {
  if (reason != nullptr && reasonLen > 0U) reason[0] = '\0';
  refreshCore0OutputMirror();
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
    if (channelMask[ch] != 0U) {
      if (reason != nullptr && reasonLen > 0U) snprintf(reason, reasonLen, "All outputs must be OFF before policy installation");
      return false;
    }
  }

  CoreSafetySnapshot snapshot{};
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
    snapshot.minimumOhms[ch] = float(safetyMinOhm[ch]);
    snapshot.maximumOhms[ch] = float(safetyMaxOhm[ch]);
    snapshot.maximumActiveBits[ch] = safetyMaxActiveBits[ch];
    for (uint8_t bit = 0; bit < BIT_COUNT; ++bit) snapshot.resistorOhms[ch][bit] = channelResistorOhms[ch][bit];
  }
  snapshot.expertMode = safetyExpertMode ? 1U : 0U;

  uint8_t slot = 0;
  uint32_t generation = 0;
  if (!coreTransportStagePolicySnapshot(snapshot, slot, generation)) {
    if (reason != nullptr && reasonLen > 0U) snprintf(reason, reasonLen, "Unable to stage immutable Core 1 policy snapshot");
    return false;
  }

  CoreCommand command{};
  command.type = CORE_CMD_INSTALL_POLICY;
  command.policySlot = slot;
  CoreResult result{};
  const bool ok = submitCoreCommandWait(command, result, CORE_COMMAND_TIMEOUT_MS);
  if (!ok && reason != nullptr && reasonLen > 0U) snprintf(reason, reasonLen, "%s (detail=%u)", responseStatusText(result.status, result.detail), unsigned(result.detail));
  return ok;
}

bool applyChannelMask(uint8_t channelIndex, uint16_t newMask) {
  char reason[128] = {0};
  const bool ok = requestSetChannelMask(channelIndex, newMask, reason, sizeof(reason));
  if (!ok) setLastError(reason[0] ? reason : "-500,\"Core 1 apply failed\"");
  return ok;
}

bool applyAllMasksSafely(const uint16_t masks[CHANNEL_COUNT], char* reason, size_t reasonLen) {
  return requestSetAllMasks(masks, reason, reasonLen);
}

bool forceAllOff(char* reason, size_t reasonLen) {
  char localReason[128] = {0};
  const bool ok = requestAllOff(localReason, sizeof(localReason));
  if (!ok) {
    const char* message = localReason[0] ? localReason : "Core 1 all-off failed";
    setLastError(message);
    setLedMode(LED_FAULT);
    if (reason != nullptr && reasonLen > 0U) snprintf(reason, reasonLen, "%s", message);
    return false;
  }
  refreshCore0OutputMirror();
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
    if (channelMask[ch] != 0U) {
      const char* message = "Core 1 all-off snapshot verification failed";
      setLastError(message);
      setLedMode(LED_FAULT);
      if (reason != nullptr && reasonLen > 0U) snprintf(reason, reasonLen, "%s", message);
      return false;
    }
  }
  setStatus("All channels OFF");
  if (!fatalSafeStateActive) setLedMode(LED_OK);
  if (reason != nullptr && reasonLen > 0U) reason[0] = '\0';
  return true;
}

void getCoreTransportDiagnostics(CoreTransportDiagnostics& diagnostics) {
  coreTransportGetDiagnostics(diagnostics);
}
