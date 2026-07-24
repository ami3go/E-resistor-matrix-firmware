/**
 * @file core1_runtime.cpp
 * @brief Arduino Core 1 startup and loop entry points.
 */
#include "core1_engine.h"

void setup1() {
  coreTransportSetCore1StartupStage(CORE1_STARTUP_SETUP_ENTERED);

  const bool gpioReady = core1PhysicalInitialize();
  coreTransportSetCore1StartupStage(gpioReady
      ? CORE1_STARTUP_GPIO_READY
      : CORE1_STARTUP_GPIO_FAILED);

  const bool offOk = gpioReady && core1ForceAllOffPhysical(0);
  core1OutputsReady = offOk && core1PhysicalOutputsSafe();
  core1EngineReady = core1OutputsReady;
  core1Fault = !core1OutputsReady;
  core1HeartbeatMs = millis();
  if (!gpioReady) {
    coreTransportSetCore1StartupStage(CORE1_STARTUP_GPIO_FAILED);
  } else if (!offOk) {
    coreTransportSetCore1StartupStage(CORE1_STARTUP_ALL_OFF_FAILED);
  } else {
    coreTransportSetCore1StartupStage(CORE1_STARTUP_OUTPUTS_OFF);
  }

  // Core 0 initializes all shared locks and queues as its first setup step.
  // Core 1 keeps retrying, but also publishes a stage value so startup
  // diagnostics distinguish a transport wait from a GPIO/all-OFF failure.
  if (offOk) coreTransportSetCore1StartupStage(CORE1_STARTUP_WAITING_FOR_TRANSPORT);
  while (!coreTransportWaitUntilInitialized(CORE_STARTUP_TIMEOUT_MS)) {
    core1HeartbeatMs = millis();
  }
  if (offOk) coreTransportSetCore1StartupStage(CORE1_STARTUP_TRANSPORT_READY);

  CoreOutputSnapshot snapshot{};
  snapshot.snapshotSequence = 1;
  snapshot.safetyGeneration = coreTransportCurrentGeneration();
  snapshot.flags = core1OutputsReady
      ? (CORE_SNAPSHOT_OUTPUTS_SAFE | CORE_SNAPSHOT_ENGINE_READY)
      : CORE_SNAPSHOT_FAULT;
  coreTransportPublishSnapshot(snapshot);
  if (offOk) coreTransportSetCore1StartupStage(CORE1_STARTUP_SNAPSHOT_PUBLISHED);
  core1EmitEvent(offOk ? CORE1_EVT_ENGINE_READY : CORE1_EVT_ALL_OFF_FAILED,
                 offOk ? FW_LOG_INFO : FW_LOG_ERROR, 0xFF, 0,
                 offOk ? CORE_DETAIL_NONE : CORE_DETAIL_PHYSICAL_ALL_OFF_FAILED, 0, 0);
  coreTransportSignalCore1Ready();

  const int freeStack = rp2040.getFreeStack();
  if (freeStack > 0) core1MinFreeStackBytes = uint32_t(freeStack);
}

void loop1() {
  static bool firstLoop = true;
  if (firstLoop) {
    firstLoop = false;
    coreTransportSetCore1StartupStage(CORE1_STARTUP_LOOP_RUNNING);
  }
  const uint32_t startUs = micros();
  core1ProcessEngineOnce();
  if ((core1LoopCounter & 0xFFU) == 0U) {
    const int freeStack = rp2040.getFreeStack();
    if (freeStack > 0 && uint32_t(freeStack) < core1MinFreeStackBytes) core1MinFreeStackBytes = uint32_t(freeStack);
  }
  const uint32_t elapsedUs = micros() - startUs;
  if (elapsedUs > core1LoopMaxUs) core1LoopMaxUs = elapsedUs;
  delayMicroseconds(100);
}
