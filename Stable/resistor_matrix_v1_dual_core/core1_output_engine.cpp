/**
 * @file core1_output_engine.cpp
 * @brief Deterministic Core 1 command validator and physical-output dispatcher.
 */
#include "core1_engine.h"
#include <math.h>

static CoreSafetySnapshot s_activePolicy{};
static bool s_policyInstalled = false;
static uint32_t s_snapshotSequence = 0;
static uint32_t s_lastCommandSequence = 0;
static uint32_t s_activeGeneration = 1;
static bool s_core0FailsafeLatched = false;

#ifdef ERESISTOR_TEST_MODE
static volatile uint32_t s_testNextDelayMs = 0;
static volatile uint32_t s_testPauseUntilMs = 0;
static volatile bool s_testInvalidateNextCommand = false;
void core1TestSetNextCommandDelayMs(uint32_t delayMs) { s_testNextDelayMs = delayMs; }
void core1TestPauseProcessingMs(uint32_t pauseMs) { s_testPauseUntilMs = millis() + pauseMs; }
void core1TestInvalidateNextCommandGeneration() { s_testInvalidateNextCommand = true; }
#endif

static bool deadlineExpired(uint32_t nowUs, uint32_t deadlineAtUs) {
  return int32_t(nowUs - deadlineAtUs) >= 0;
}

static void publishState(uint32_t commandSequence) {
  CoreOutputSnapshot snapshot{};
  snapshot.snapshotSequence = ++s_snapshotSequence;
  snapshot.lastCommandSequence = commandSequence;
  snapshot.safetyGeneration = s_activeGeneration;
  const uint16_t* masks = core1PhysicalMasks();
  const uint32_t* counters = core1PhysicalApplyCounters();
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
    snapshot.masks[ch] = masks[ch];
    snapshot.applyCounter[ch] = counters[ch];
  }
  if (core1PhysicalOutputsSafe()) snapshot.flags |= CORE_SNAPSHOT_OUTPUTS_SAFE;
  if (core1EngineReady) snapshot.flags |= CORE_SNAPSHOT_ENGINE_READY;
  if (core1Fault) snapshot.flags |= CORE_SNAPSHOT_FAULT;
  if (s_policyInstalled) snapshot.flags |= CORE_SNAPSHOT_POLICY_INSTALLED;
  coreTransportPublishSnapshot(snapshot);
}

static bool allOutputsOff() {
  const uint16_t* masks = core1PhysicalMasks();
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) if (masks[ch] != 0U) return false;
  return true;
}

static CoreResponseStatus validateMask(uint8_t channelIndex, uint16_t mask, uint16_t& detail) {
  detail = CORE_DETAIL_NONE;
  if (channelIndex >= CHANNEL_COUNT) {
    detail = CORE_DETAIL_INVALID_CHANNEL;
    return CORE_RESP_INVALID_ARGUMENT;
  }
  if (mask == 0U) return CORE_RESP_OK;
  if (!s_policyInstalled) return CORE_RESP_POLICY_NOT_INSTALLED;
  const uint8_t activeBits = uint8_t(__builtin_popcount(mask));
  if (!s_activePolicy.expertMode && activeBits > s_activePolicy.maximumActiveBits[channelIndex]) {
    detail = CORE_DETAIL_ACTIVE_BIT_LIMIT;
    return CORE_RESP_REJECTED;
  }
  float conductance = 0.0f;
  uint16_t remaining = mask;
  while (remaining != 0U) {
    const uint8_t bit = uint8_t(__builtin_ctz(remaining));
    remaining &= uint16_t(remaining - 1U);
    const float resistance = s_activePolicy.resistorOhms[channelIndex][bit];
    if (!isfinite(resistance) || resistance <= 0.0f) {
      detail = CORE_DETAIL_RESISTANCE_INVALID;
      return CORE_RESP_REJECTED;
    }
    conductance += 1.0f / resistance;
  }
  if (!isfinite(conductance) || conductance <= 0.0f) {
    detail = CORE_DETAIL_RESISTANCE_INVALID;
    return CORE_RESP_REJECTED;
  }
  if (!s_activePolicy.expertMode) {
    const float equivalent = 1.0f / conductance;
    if (equivalent < s_activePolicy.minimumOhms[channelIndex]) {
      detail = CORE_DETAIL_RESISTANCE_BELOW_MIN;
      return CORE_RESP_REJECTED;
    }
    const float maximum = s_activePolicy.maximumOhms[channelIndex];
    if (maximum > 0.0f && equivalent > maximum * 1.000001f) {
      detail = CORE_DETAIL_RESISTANCE_ABOVE_MAX;
      return CORE_RESP_REJECTED;
    }
  }
  return CORE_RESP_OK;
}

static void complete(const CoreCommand& command, CoreResponseStatus status, uint16_t detail) {
  publishState(command.sequence);
  CoreResult result{};
  result.sequence = command.sequence;
  result.snapshotSequence = s_snapshotSequence;
  result.completedAtUs = micros();
  result.status = status;
  result.detail = detail;
  result.channelIndex = command.channelIndex;
  coreTransportPushResult(result);
}

static bool validateEnvelope(const CoreCommand& command, CoreResponseStatus& status, uint16_t& detail) {
  const uint32_t nowUs = micros();
  if (core1Fault && (command.type == CORE_CMD_SET_MASK || command.type == CORE_CMD_SET_ALL_MASKS)) {
    status = CORE_RESP_ENGINE_NOT_READY;
    detail = CORE_DETAIL_SHIFT_REGISTER_NOT_READY;
    return false;
  }
  if (deadlineExpired(nowUs, command.deadlineAtUs)) {
    status = CORE_RESP_EXPIRED;
    detail = CORE_DETAIL_NONE;
    coreTransportRecordExpired();
    core1EmitEvent(CORE1_EVT_COMMAND_EXPIRED, FW_LOG_ERROR, command.channelIndex,
                   command.mask, detail, 0, command.sequence);
    return false;
  }
  const uint32_t currentGeneration = coreTransportCurrentGeneration();
  if (command.safetyGeneration != currentGeneration) {
    status = CORE_RESP_GENERATION_MISMATCH;
    detail = CORE_DETAIL_NONE;
    coreTransportRecordGenerationReject();
    core1EmitEvent(CORE1_EVT_COMMAND_INVALIDATED, FW_LOG_ERROR, command.channelIndex,
                   command.mask, detail, 0, command.sequence);
    return false;
  }
  if (command.type != CORE_CMD_INSTALL_POLICY && command.type != CORE_CMD_CLEAR_ALL &&
      command.type != CORE_CMD_GET_SNAPSHOT && s_activeGeneration != command.safetyGeneration) {
    status = CORE_RESP_POLICY_NOT_INSTALLED;
    detail = CORE_DETAIL_NONE;
    return false;
  }
  return true;
}

static void processCommand(const CoreCommand& command) {
  s_lastCommandSequence = command.sequence;
  core1LastCommandMs = millis();
  core1CommandCounter++;

#ifdef ERESISTOR_TEST_MODE
  const uint32_t delayMs = s_testNextDelayMs;
  s_testNextDelayMs = 0;
  if (delayMs > 0U) delay(delayMs);
  if (s_testInvalidateNextCommand) {
    s_testInvalidateNextCommand = false;
    coreTransportInvalidateGeneration();
  }
#endif

  CoreResponseStatus status = CORE_RESP_OK;
  uint16_t detail = CORE_DETAIL_NONE;
  if (!validateEnvelope(command, status, detail)) {
    complete(command, status, detail);
    return;
  }

  switch (command.type) {
    case CORE_CMD_INSTALL_POLICY: {
      if (!allOutputsOff()) {
        complete(command, CORE_RESP_REJECTED, CORE_DETAIL_POLICY_INSTALL_REQUIRES_OFF);
        return;
      }
      CoreSafetySnapshot candidate{};
      if (!coreTransportReadPolicySnapshot(command.policySlot, command.safetyGeneration, candidate)) {
        complete(command, CORE_RESP_INVALID_ARGUMENT, CORE_DETAIL_POLICY_SLOT_INVALID);
        return;
      }
      s_activePolicy = candidate;
      s_activeGeneration = command.safetyGeneration;
      s_policyInstalled = true;
      coreTransportRecordPolicyInstall();
      core1EmitEvent(CORE1_EVT_POLICY_INSTALLED, FW_LOG_INFO, 0xFF, 0, command.policySlot, 0, command.sequence);
      complete(command, CORE_RESP_OK, CORE_DETAIL_NONE);
      return;
    }

    case CORE_CMD_SET_MASK:
      status = validateMask(command.channelIndex, command.mask, detail);
      if (status != CORE_RESP_OK) {
        core1EmitEvent(CORE1_EVT_APPLY_REJECTED, FW_LOG_ERROR, command.channelIndex,
                       command.mask, detail, 0, command.sequence);
        complete(command, status, detail);
        return;
      }
      if (!core1ApplyChannelMaskPhysical(command.channelIndex, command.mask, command.sequence)) {
        core1Fault = true;
        core1ForceAllOffPhysical(command.sequence, CORE_DETAIL_PHYSICAL_APPLY_FAILED);
        complete(command, CORE_RESP_IO_ERROR, CORE_DETAIL_PHYSICAL_APPLY_FAILED);
        return;
      }
      complete(command, CORE_RESP_OK, CORE_DETAIL_NONE);
      return;

    case CORE_CMD_SET_ALL_MASKS:
      for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
        status = validateMask(ch, command.masks[ch], detail);
        if (status != CORE_RESP_OK) {
          core1EmitEvent(CORE1_EVT_PROFILE_FAILED, FW_LOG_ERROR, ch, command.masks[ch], detail, 0, command.sequence);
          complete(command, status, detail);
          return;
        }
      }
      if (!core1ApplyAllMasksPhysical(command.masks, command.sequence)) {
        core1Fault = true;
        complete(command, CORE_RESP_IO_ERROR, CORE_DETAIL_PHYSICAL_APPLY_FAILED);
        return;
      }
      complete(command, CORE_RESP_OK, CORE_DETAIL_NONE);
      return;

    case CORE_CMD_CLEAR_ALL:
      if (!core1ForceAllOffPhysical(command.sequence)) {
        core1Fault = true;
        complete(command, CORE_RESP_IO_ERROR, CORE_DETAIL_PHYSICAL_ALL_OFF_FAILED);
      } else {
        complete(command, CORE_RESP_OK, CORE_DETAIL_NONE);
      }
      return;

    case CORE_CMD_GET_SNAPSHOT:
      complete(command, CORE_RESP_OK, CORE_DETAIL_NONE);
      return;

    case CORE_CMD_NONE:
    default:
      coreTransportRecordInvalidCommand();
      complete(command, CORE_RESP_INVALID_ARGUMENT, CORE_DETAIL_INVALID_COMMAND);
      return;
  }
}

void core1ProcessEngineOnce() {
  core1HeartbeatMs = millis();
  core1LoopCounter++;

  if (emergencyOffRequested) {
    const bool ok = core1ForceAllOffPhysical(s_lastCommandSequence);
    emergencyOffRequested = false;
    core1OutputsReady = ok && core1PhysicalOutputsSafe();
    if (!ok) core1Fault = true;
    publishState(s_lastCommandSequence);
  }

  if (!s_core0FailsafeLatched && coreTransportCore0HeartbeatExpired(millis()) && !allOutputsOff()) {
    s_core0FailsafeLatched = true;
    coreTransportInvalidateGeneration();
    coreTransportRecordCore0Failsafe();
    core1ForceAllOffPhysical(s_lastCommandSequence, CORE_DETAIL_CORE0_HEARTBEAT_EXPIRED);
    core1EmitEvent(CORE1_EVT_CORE0_FAILSAFE, FW_LOG_ERROR, 0xFF, 0,
                   CORE_DETAIL_CORE0_HEARTBEAT_EXPIRED, 0, s_lastCommandSequence);
    publishState(s_lastCommandSequence);
  }
  if (!coreTransportCore0HeartbeatExpired(millis())) s_core0FailsafeLatched = false;

#ifdef ERESISTOR_TEST_MODE
  if (int32_t(millis() - s_testPauseUntilMs) < 0) return;
#endif

  CoreCommand command{};
  if (coreTransportPopCommand(command)) processCommand(command);
  core1OutputsReady = core1PhysicalOutputsSafe();
}
