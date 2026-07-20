/**
 * @file core_transport.cpp
 * @brief Bounded cross-core queues, semaphores, generation control, and atomic snapshots.
 */
#include <Arduino.h>
#include <pico/sync.h>
#include <string.h>
#include "core_transport_types.h"

static CoreCommand s_commandQueue[CORE_COMMAND_QUEUE_DEPTH];
static CoreResult s_resultQueue[CORE_RESULT_QUEUE_DEPTH];
static uint8_t s_commandHead = 0, s_commandTail = 0, s_commandCount = 0;
static uint8_t s_resultHead = 0, s_resultTail = 0, s_resultCount = 0;

static spin_lock_t* s_commandLock = nullptr;
static spin_lock_t* s_resultLock = nullptr;
static spin_lock_t* s_stateLock = nullptr;
static spin_lock_t* s_policyLock = nullptr;
static spin_lock_t* s_generationLock = nullptr;
static spin_lock_t* s_diagLock = nullptr;
static semaphore_t s_resultReady;
static semaphore_t s_core1Ready;
static volatile bool s_initialized = false;

static uint32_t s_nextSequence = 1;
static uint32_t s_safetyGeneration = 1;
static CoreOutputSnapshot s_outputSnapshot{};
static CoreSafetySnapshot s_policySlots[CORE_POLICY_SLOT_COUNT]{};
static bool s_policySlotValid[CORE_POLICY_SLOT_COUNT] = {false, false};
static uint8_t s_nextPolicySlot = 0;
static volatile uint32_t s_core0HeartbeatMs = 0;
static CoreTransportDiagnostics s_diag{};

static uint32_t lockOrDisable(spin_lock_t* lock) {
  if (lock == nullptr) {
    noInterrupts();
    return 0;
  }
  return spin_lock_blocking(lock);
}

static void unlockOrEnable(spin_lock_t* lock, uint32_t irqState) {
  if (lock == nullptr) {
    interrupts();
    return;
  }
  spin_unlock(lock, irqState);
}

void coreTransportInitialize() {
  if (s_initialized) return;
  s_commandLock = spin_lock_init(spin_lock_claim_unused(true));
  s_resultLock = spin_lock_init(spin_lock_claim_unused(true));
  s_stateLock = spin_lock_init(spin_lock_claim_unused(true));
  s_policyLock = spin_lock_init(spin_lock_claim_unused(true));
  s_generationLock = spin_lock_init(spin_lock_claim_unused(true));
  s_diagLock = spin_lock_init(spin_lock_claim_unused(true));

  s_commandHead = s_commandTail = s_commandCount = 0;
  s_resultHead = s_resultTail = s_resultCount = 0;
  s_nextSequence = 1;
  s_safetyGeneration = 1;
  memset(&s_outputSnapshot, 0, sizeof(s_outputSnapshot));
  memset(&s_policySlots, 0, sizeof(s_policySlots));
  s_policySlotValid[0] = s_policySlotValid[1] = false;
  memset(&s_diag, 0, sizeof(s_diag));
  s_diag.currentSafetyGeneration = s_safetyGeneration;
  sem_init(&s_resultReady, 0, CORE_RESULT_QUEUE_DEPTH);
  sem_init(&s_core1Ready, 0, 1);
  s_core0HeartbeatMs = millis();
  s_initialized = true;
  __sev();
}

bool coreTransportIsInitialized() { return s_initialized; }

bool coreTransportWaitUntilInitialized(uint32_t timeoutMs) {
  const uint32_t start = millis();
  while (!s_initialized && (millis() - start) < timeoutMs) {
    __wfe();
  }
  return s_initialized;
}

uint32_t coreTransportAllocateSequence() {
  uint32_t irq = lockOrDisable(s_generationLock);
  uint32_t sequence = s_nextSequence++;
  if (s_nextSequence == 0U) s_nextSequence = 1U;
  if (sequence == 0U) sequence = s_nextSequence++;
  unlockOrEnable(s_generationLock, irq);
  irq = lockOrDisable(s_diagLock);
  s_diag.lastSubmittedSequence = sequence;
  unlockOrEnable(s_diagLock, irq);
  return sequence;
}

uint32_t coreTransportCurrentGeneration() {
  uint32_t irq = lockOrDisable(s_generationLock);
  uint32_t value = s_safetyGeneration;
  unlockOrEnable(s_generationLock, irq);
  return value;
}

uint32_t coreTransportInvalidateGeneration() {
  uint32_t irq = lockOrDisable(s_generationLock);
  s_safetyGeneration++;
  if (s_safetyGeneration == 0U) s_safetyGeneration = 1U;
  uint32_t value = s_safetyGeneration;
  unlockOrEnable(s_generationLock, irq);
  irq = lockOrDisable(s_diagLock);
  s_diag.currentSafetyGeneration = value;
  unlockOrEnable(s_diagLock, irq);
  return value;
}

bool coreTransportPushCommand(const CoreCommand& command) {
  if (!s_initialized) return false;
  bool ok = false;
  uint32_t irq = lockOrDisable(s_commandLock);
  if (s_commandCount < CORE_COMMAND_QUEUE_DEPTH) {
    s_commandQueue[s_commandTail] = command;
    s_commandTail = uint8_t((s_commandTail + 1U) % CORE_COMMAND_QUEUE_DEPTH);
    s_commandCount++;
    ok = true;
  }
  unlockOrEnable(s_commandLock, irq);
  if (!ok) {
    irq = lockOrDisable(s_diagLock);
    s_diag.commandQueueOverflowCount++;
    unlockOrEnable(s_diagLock, irq);
  }
  return ok;
}

bool coreTransportPopCommand(CoreCommand& command) {
  bool ok = false;
  uint32_t irq = lockOrDisable(s_commandLock);
  if (s_commandCount > 0U) {
    command = s_commandQueue[s_commandHead];
    s_commandHead = uint8_t((s_commandHead + 1U) % CORE_COMMAND_QUEUE_DEPTH);
    s_commandCount--;
    ok = true;
  }
  unlockOrEnable(s_commandLock, irq);
  return ok;
}

bool coreTransportPushResult(const CoreResult& result) {
  bool ok = false;
  uint32_t irq = lockOrDisable(s_resultLock);
  if (s_resultCount < CORE_RESULT_QUEUE_DEPTH) {
    s_resultQueue[s_resultTail] = result;
    s_resultTail = uint8_t((s_resultTail + 1U) % CORE_RESULT_QUEUE_DEPTH);
    s_resultCount++;
    ok = true;
  }
  unlockOrEnable(s_resultLock, irq);
  irq = lockOrDisable(s_diagLock);
  if (ok) s_diag.lastCompletedSequence = result.sequence;
  else s_diag.resultQueueOverflowCount++;
  unlockOrEnable(s_diagLock, irq);
  if (ok) sem_release(&s_resultReady);
  return ok;
}

static bool popResult(CoreResult& result) {
  bool ok = false;
  uint32_t irq = lockOrDisable(s_resultLock);
  if (s_resultCount > 0U) {
    result = s_resultQueue[s_resultHead];
    s_resultHead = uint8_t((s_resultHead + 1U) % CORE_RESULT_QUEUE_DEPTH);
    s_resultCount--;
    ok = true;
  }
  unlockOrEnable(s_resultLock, irq);
  return ok;
}

bool coreTransportWaitForResult(CoreResult& result, uint32_t timeoutMs) {
  if (!s_initialized || !sem_acquire_timeout_ms(&s_resultReady, timeoutMs)) return false;
  return popResult(result);
}

void coreTransportSignalCore1Ready() {
  if (s_initialized) sem_release(&s_core1Ready);
}

bool coreTransportWaitForCore1Ready(uint32_t timeoutMs) {
  return s_initialized && sem_acquire_timeout_ms(&s_core1Ready, timeoutMs);
}

void coreTransportPublishSnapshot(const CoreOutputSnapshot& snapshot) {
  uint32_t irq = lockOrDisable(s_stateLock);
  s_outputSnapshot = snapshot;
  unlockOrEnable(s_stateLock, irq);
}

bool coreTransportReadSnapshot(CoreOutputSnapshot& snapshot) {
  if (!s_initialized) return false;
  uint32_t irq = lockOrDisable(s_stateLock);
  snapshot = s_outputSnapshot;
  unlockOrEnable(s_stateLock, irq);
  return true;
}

bool coreTransportStagePolicySnapshot(const CoreSafetySnapshot& input, uint8_t& slot, uint32_t& generation) {
  CoreOutputSnapshot state{};
  if (!coreTransportReadSnapshot(state)) return false;
  if ((state.flags & CORE_SNAPSHOT_OUTPUTS_SAFE) == 0U ||
      (state.flags & CORE_SNAPSHOT_ENGINE_READY) == 0U) return false;
  for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
    if (state.masks[ch] != 0U) return false;
  }

  generation = coreTransportInvalidateGeneration();
  uint32_t irq = lockOrDisable(s_policyLock);
  slot = s_nextPolicySlot;
  s_nextPolicySlot = uint8_t((s_nextPolicySlot + 1U) % CORE_POLICY_SLOT_COUNT);
  s_policySlots[slot] = input;
  s_policySlots[slot].safetyGeneration = generation;
  s_policySlotValid[slot] = true;
  unlockOrEnable(s_policyLock, irq);
  return true;
}

bool coreTransportReadPolicySnapshot(uint8_t slot, uint32_t expectedGeneration, CoreSafetySnapshot& snapshot) {
  if (slot >= CORE_POLICY_SLOT_COUNT) return false;
  bool ok = false;
  uint32_t irq = lockOrDisable(s_policyLock);
  if (s_policySlotValid[slot] && s_policySlots[slot].safetyGeneration == expectedGeneration) {
    snapshot = s_policySlots[slot];
    ok = true;
  }
  unlockOrEnable(s_policyLock, irq);
  return ok;
}

void coreTransportKickCore0Heartbeat() { s_core0HeartbeatMs = millis(); }

bool coreTransportCore0HeartbeatExpired(uint32_t nowMs) {
  return s_initialized && (nowMs - s_core0HeartbeatMs) > CORE0_FAILSAFE_TIMEOUT_MS;
}

void coreTransportGetDiagnostics(CoreTransportDiagnostics& out) {
  uint32_t irq = lockOrDisable(s_diagLock);
  out = s_diag;
  unlockOrEnable(s_diagLock, irq);
  out.currentSafetyGeneration = coreTransportCurrentGeneration();
}

static void incrementDiagnostic(uint32_t CoreTransportDiagnostics::*field) {
  uint32_t irq = lockOrDisable(s_diagLock);
  s_diag.*field += 1U;
  unlockOrEnable(s_diagLock, irq);
}

void coreTransportRecordTimeout() { incrementDiagnostic(&CoreTransportDiagnostics::commandTimeoutCount); }
void coreTransportRecordExpired() { incrementDiagnostic(&CoreTransportDiagnostics::commandExpiredCount); }
void coreTransportRecordGenerationReject() { incrementDiagnostic(&CoreTransportDiagnostics::generationRejectCount); }
void coreTransportRecordInvalidCommand() { incrementDiagnostic(&CoreTransportDiagnostics::invalidCommandCount); }
void coreTransportRecordPolicyInstall() { incrementDiagnostic(&CoreTransportDiagnostics::policyInstallCount); }
void coreTransportRecordCore0Failsafe() { incrementDiagnostic(&CoreTransportDiagnostics::core0FailsafeCount); }
