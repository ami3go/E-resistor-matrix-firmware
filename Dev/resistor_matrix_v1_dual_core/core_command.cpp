/**
 * @file core_command.cpp
 * @brief Dual-core command queue, bounded request/response handling, and Core 1 hardware command dispatcher.
 */

#include "app.h"

// ============================================================
// Dual-core command queue
//
// Core 0 owns Ethernet, HTTP, SCPI, parsing, files, and UI.
// Core 1 owns every physical shift-register output operation.
//
// This Arduino-prototype implementation uses small fixed ring queues and
// RP2040 hardware spin locks. Avoid GCC __atomic builtins here: on this
// Arduino RP2040 toolchain they can pull unresolved libatomic symbols such
// as __atomic_test_and_set at link time.
// ============================================================

static CoreCommand commandQueue[CORE_COMMAND_QUEUE_DEPTH];
static uint8_t commandHead = 0;
static uint8_t commandTail = 0;
static uint8_t commandCount = 0;

static CoreResponse responseQueue[CORE_RESPONSE_QUEUE_DEPTH];
static uint8_t responseHead = 0;
static uint8_t responseTail = 0;
static uint8_t responseCount = 0;

static uint32_t nextRequestId = 1;

static spin_lock_t* commandSpinLock = nullptr;
static spin_lock_t* responseSpinLock = nullptr;

/**
 * @brief Initialize the dual-core command engine and its RP2040 hardware spin lock.
 */
void initCoreCommandEngine() {
  // Claim two RP2040 hardware spinlocks for cross-core queue protection.
  // It is safe to call this more than once; only the first call initializes.
  if (commandSpinLock == nullptr) {
    uint commandLockNum = spin_lock_claim_unused(true);
    commandSpinLock = spin_lock_init(commandLockNum);
  }
  if (responseSpinLock == nullptr) {
    uint responseLockNum = spin_lock_claim_unused(true);
    responseSpinLock = spin_lock_init(responseLockNum);
  }

  uint32_t commandIrq = spin_lock_blocking(commandSpinLock);
  commandHead = 0;
  commandTail = 0;
  commandCount = 0;
  spin_unlock(commandSpinLock, commandIrq);

  uint32_t responseIrq = spin_lock_blocking(responseSpinLock);
  responseHead = 0;
  responseTail = 0;
  responseCount = 0;
  spin_unlock(responseSpinLock, responseIrq);

  nextRequestId = 1;
}

/**
 * @brief Acquire the shared command/response queue lock.
 * @param lock Function parameter.
 * @return Result value; for bool, true means the operation succeeded.
 */
static uint32_t lockQueue(spin_lock_t* lock) {
  if (lock == nullptr) {
    // Should not happen when setup() calls initCoreCommandEngine() first.
    // Keep a local-core fallback instead of crashing during early boot.
    noInterrupts();
    return 0;
  }
  return spin_lock_blocking(lock);
}

/**
 * @brief Release the shared command/response queue lock.
 * @param lock Function parameter.
 * @param irqState Function parameter.
 * @return Result value; for bool, true means the operation succeeded.
 */
static void unlockQueue(spin_lock_t* lock, uint32_t irqState) {
  if (lock == nullptr) {
    interrupts();
    return;
  }
  spin_unlock(lock, irqState);
}

/**
 * @brief Allocate a non-zero request identifier for a Core 0 to Core 1 command.
 * @return Result value; for bool, true means the operation succeeded.
 */
static uint32_t allocateRequestId() {
  // Core 0 is the only caller in the current architecture, so a plain
  // increment is sufficient and avoids libatomic dependencies.
  uint32_t id = nextRequestId++;
  if (nextRequestId == 0U) {
    nextRequestId = 1U;
  }
  if (id == 0U) {
    id = nextRequestId++;
  }
  return id;
}

/**
 * @brief Push one command into the Core 1 command queue.
 * @param command Command object to submit or process.
 * @return Result value; for bool, true means the operation succeeded.
 */
static bool pushCommand(const CoreCommand& command) {
  bool ok = false;
  uint32_t irqState = lockQueue(commandSpinLock);
  if (commandCount < CORE_COMMAND_QUEUE_DEPTH) {
    commandQueue[commandTail] = command;
    commandTail = uint8_t((commandTail + 1U) % CORE_COMMAND_QUEUE_DEPTH);
    commandCount++;
    ok = true;
  }
  unlockQueue(commandSpinLock, irqState);

  if (!ok) {
    core1QueueOverflowCounter++;
  }
  return ok;
}

/**
 * @brief Pop one pending command from the Core 1 command queue.
 * @param command Command object to submit or process.
 * @return Result value; for bool, true means the operation succeeded.
 */
static bool popCommand(CoreCommand& command) {
  bool ok = false;
  uint32_t irqState = lockQueue(commandSpinLock);
  if (commandCount > 0U) {
    command = commandQueue[commandHead];
    commandHead = uint8_t((commandHead + 1U) % CORE_COMMAND_QUEUE_DEPTH);
    commandCount--;
    ok = true;
  }
  unlockQueue(commandSpinLock, irqState);
  return ok;
}

/**
 * @brief Push one command response into the Core 0 response queue.
 * @param response Response object to fill or inspect.
 * @return Result value; for bool, true means the operation succeeded.
 */
static bool pushResponse(const CoreResponse& response) {
  bool ok = false;
  uint32_t irqState = lockQueue(responseSpinLock);
  if (responseCount < CORE_RESPONSE_QUEUE_DEPTH) {
    responseQueue[responseTail] = response;
    responseTail = uint8_t((responseTail + 1U) % CORE_RESPONSE_QUEUE_DEPTH);
    responseCount++;
    ok = true;
  }
  unlockQueue(responseSpinLock, irqState);

  if (!ok) {
    core1QueueOverflowCounter++;
  }
  return ok;
}

/**
 * @brief Find and remove the response matching a request identifier.
 * @param response Response object to fill or inspect.
 * @return Result value; for bool, true means the operation succeeded.
 */
static bool popResponse(CoreResponse& response) {
  bool ok = false;
  uint32_t irqState = lockQueue(responseSpinLock);
  if (responseCount > 0U) {
    response = responseQueue[responseHead];
    responseHead = uint8_t((responseHead + 1U) % CORE_RESPONSE_QUEUE_DEPTH);
    responseCount--;
    ok = true;
  }
  unlockQueue(responseSpinLock, irqState);
  return ok;
}

/**
 * @brief Build a CoreResponse structure from command execution data.
 * @param response Response object to fill or inspect.
 * @param command Command object to submit or process.
 * @param status Function parameter.
 * @param message Function parameter.
 * @return Result value; for bool, true means the operation succeeded.
 */
static void fillResponse(
  CoreResponse& response,
  const CoreCommand& command,
  CoreResponseStatus status,
  const char* message
) {
  memset(&response, 0, sizeof(response));
  response.requestId = command.requestId;
  response.status = status;
  response.channelIndex = command.channelIndex;
  response.mask = command.mask;
  for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
    response.masks[i] = channelMask[i];
  }
  if (message != nullptr) {
    strncpy(response.message, message, sizeof(response.message) - 1);
    response.message[sizeof(response.message) - 1] = '\0';
  }
}

/**
 * @brief Submit a command to Core 1 and wait for the matching response with a timeout.
 * @param command Command object to submit or process.
 * @param response Response object to fill or inspect.
 * @param timeoutMs Maximum time to wait before reporting timeout.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool submitCoreCommandWait(CoreCommand& command, CoreResponse& response, uint32_t timeoutMs) {
  memset(&response, 0, sizeof(response));

  if (!core1EngineReady) {
    response.status = CORE_RESP_TIMEOUT;
    strncpy(response.message, "Core 1 hardware engine not ready", sizeof(response.message) - 1);
    return false;
  }

  command.requestId = allocateRequestId();

  if (!pushCommand(command)) {
    response.requestId = command.requestId;
    response.status = CORE_RESP_BUSY;
    strncpy(response.message, "Core command queue full", sizeof(response.message) - 1);
    return false;
  }

  uint32_t startMs = millis();
  while ((millis() - startMs) < timeoutMs) {
    CoreResponse candidate;
    if (popResponse(candidate)) {
      if (candidate.requestId == command.requestId) {
        response = candidate;
        return response.status == CORE_RESP_OK;
      }
      // There should normally be only one outstanding Core-0 request at a
      // time. If an older unmatched response appears, discard it instead of
      // blocking the current command forever.
    }

    updateHeartbeat();
    delay(1);
  }

  response.requestId = command.requestId;
  response.status = CORE_RESP_TIMEOUT;
  strncpy(response.message, "Core 1 command timeout", sizeof(response.message) - 1);
  return false;
}

/**
 * @brief Request Core 1 to apply one 16-bit resistance mask to one channel.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @param mask 16-bit resistance switch mask.
 * @param reason Output buffer for a human-readable diagnostic message.
 * @param reasonLen Output buffer for a human-readable diagnostic message.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool requestSetChannelMask(uint8_t channelIndex, uint16_t mask, char* reason, size_t reasonLen) {
  if (reasonLen > 0U) {
    reason[0] = '\0';
  }

  if (channelIndex >= CHANNEL_COUNT) {
    snprintf(reason, reasonLen, "invalid channel");
    return false;
  }

  CoreCommand command;
  memset(&command, 0, sizeof(command));
  command.type = CORE_CMD_SET_MASK;
  command.channelIndex = channelIndex;
  command.mask = mask;

  CoreResponse response;
  bool ok = submitCoreCommandWait(command, response, CORE_COMMAND_TIMEOUT_MS);
  if (!ok && reasonLen > 0U) {
    snprintf(reason, reasonLen, "%s", response.message[0] ? response.message : "Core 1 apply failed");
  }
  return ok;
}

/**
 * @brief Request Core 1 to apply eight channel masks after validating the full profile.
 * @param masks 16-bit resistance switch mask.
 * @param reason Output buffer for a human-readable diagnostic message.
 * @param reasonLen Output buffer for a human-readable diagnostic message.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool requestSetAllMasks(const uint16_t masks[CHANNEL_COUNT], char* reason, size_t reasonLen) {
  if (reasonLen > 0U) {
    reason[0] = '\0';
  }

  if (masks == nullptr) {
    snprintf(reason, reasonLen, "missing mask array");
    return false;
  }

  CoreCommand command;
  memset(&command, 0, sizeof(command));
  command.type = CORE_CMD_SET_ALL_MASKS;
  for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
    command.masks[i] = masks[i];
  }

  CoreResponse response;
  bool ok = submitCoreCommandWait(command, response, CORE_COMMAND_TIMEOUT_MS * 2U);
  if (!ok && reasonLen > 0U) {
    snprintf(reason, reasonLen, "%s", response.message[0] ? response.message : "Core 1 apply-all failed");
  }
  return ok;
}

/**
 * @brief Request Core 1 to switch every output channel OFF.
 * @param reason Output buffer for a human-readable diagnostic message.
 * @param reasonLen Output buffer for a human-readable diagnostic message.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool requestAllOff(char* reason, size_t reasonLen) {
  if (reasonLen > 0U) {
    reason[0] = '\0';
  }

  // Emergency safe-state does not depend on normal queue availability.
  emergencyOffRequested = true;

  uint32_t startMs = millis();
  while ((millis() - startMs) < CORE_COMMAND_TIMEOUT_MS) {
    if (!emergencyOffRequested && outputsKnownSafe) {
      return true;
    }
    updateHeartbeat();
    delay(1);
  }

  // Fall back to a normal command if the emergency flag was not acknowledged.
  CoreCommand command;
  memset(&command, 0, sizeof(command));
  command.type = CORE_CMD_CLEAR_ALL;

  CoreResponse response;
  bool ok = submitCoreCommandWait(command, response, CORE_COMMAND_TIMEOUT_MS);
  if (!ok && reasonLen > 0U) {
    snprintf(reason, reasonLen, "%s", response.message[0] ? response.message : "Core 1 all-off failed");
  }
  return ok;
}

/**
 * @brief Core-0-safe wrapper that requests a per-channel mask change on Core 1.
 * @param channelIndex Zero-based channel index unless explicitly documented as public 1-based text.
 * @param newMask 16-bit resistance switch mask.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool applyChannelMask(uint8_t channelIndex, uint16_t newMask) {
  char reason[128];
  bool ok = requestSetChannelMask(channelIndex, newMask, reason, sizeof(reason));
  if (!ok) {
    setLastError(reason[0] ? reason : "-500,\"Core 1 apply failed\"");
  }
  return ok;
}

/**
 * @brief Core-0-safe wrapper that requests an all-channel mask profile on Core 1.
 * @param masks 16-bit resistance switch mask.
 * @param reason Output buffer for a human-readable diagnostic message.
 * @param reasonLen Output buffer for a human-readable diagnostic message.
 * @return Result value; for bool, true means the operation succeeded.
 */
bool applyAllMasksSafely(const uint16_t masks[CHANNEL_COUNT], char* reason, size_t reasonLen) {
  if (reasonLen > 0U) {
    reason[0] = '\0';
  }

  if (masks == nullptr) {
    snprintf(reason, reasonLen, "missing mask array");
    return false;
  }

  // Validate complete requested state on Core 0 before queueing, then Core 1
  // validates again before touching hardware. This avoids half-applied states.
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    if (!checkMaskSafety(ch, masks[ch], reason, reasonLen)) {
      return false;
    }
  }

  return requestSetAllMasks(masks, reason, reasonLen);
}

/**
 * @brief Core-0-safe wrapper that requests an emergency/normal all-OFF transition.
 */
void forceAllOff() {
  char reason[128];
  if (!requestAllOff(reason, sizeof(reason))) {
    setLastError(reason[0] ? reason : "-500,\"Core 1 all-off failed\"");
    setLedMode(LED_FAULT);
  }
}

/**
 * @brief Execute one validated hardware command on Core 1.
 * @param command Command object to submit or process.
 * @return Result value; for bool, true means the operation succeeded.
 */
static void processCoreCommand(const CoreCommand& command) {
  CoreResponse response;
  char reason[128] = {0};

  core1LastCommandMs = millis();
  core1CommandCounter++;

  switch (command.type) {
    case CORE_CMD_SET_MASK:
      if (command.channelIndex >= CHANNEL_COUNT) {
        fillResponse(response, command, CORE_RESP_INVALID_ARGUMENT, "invalid channel");
      } else if (!checkMaskSafety(command.channelIndex, command.mask, reason, sizeof(reason))) {
        fillResponse(response, command, CORE_RESP_REJECTED, reason);
      } else if (!applyChannelMaskPhysical(command.channelIndex, command.mask)) {
        fillResponse(response, command, CORE_RESP_IO_ERROR, "physical apply failed");
      } else {
        fillResponse(response, command, CORE_RESP_OK, "OK");
      }
      pushResponse(response);
      return;

    case CORE_CMD_SET_ALL_MASKS:
      if (!applyAllMasksSafelyPhysical(command.masks, reason, sizeof(reason))) {
        fillResponse(response, command, CORE_RESP_IO_ERROR, reason[0] ? reason : "physical apply-all failed");
      } else {
        fillResponse(response, command, CORE_RESP_OK, "OK");
      }
      pushResponse(response);
      return;

    case CORE_CMD_CLEAR_ALL:
      forceAllOffPhysical();
      fillResponse(response, command, CORE_RESP_OK, "OK");
      pushResponse(response);
      return;

    case CORE_CMD_GET_MASK:
      if (command.channelIndex >= CHANNEL_COUNT) {
        fillResponse(response, command, CORE_RESP_INVALID_ARGUMENT, "invalid channel");
      } else {
        fillResponse(response, command, CORE_RESP_OK, "OK");
        response.mask = channelMask[command.channelIndex];
      }
      pushResponse(response);
      return;

    case CORE_CMD_GET_ALL_MASKS:
      fillResponse(response, command, CORE_RESP_OK, "OK");
      pushResponse(response);
      return;

    case CORE_CMD_NONE:
    default:
      fillResponse(response, command, CORE_RESP_INVALID_ARGUMENT, "invalid command");
      pushResponse(response);
      return;
  }
}

/**
 * @brief Run one iteration of the Core 1 command-processing engine.
 */
void core1ProcessEngineOnce() {
  core1HeartbeatMs = millis();
  core1LoopCounter++;

  if (emergencyOffRequested) {
    forceAllOffPhysical();
    emergencyOffRequested = false;
    core1OutputsReady = outputsKnownSafe;
  }

  CoreCommand command;
  if (popCommand(command)) {
    processCoreCommand(command);
    core1OutputsReady = outputsKnownSafe;
  }
}
