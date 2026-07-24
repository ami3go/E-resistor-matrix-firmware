/**
 * @file runtime_state_snapshot.cpp
 * @brief Gate 5 fixed-size atomic state capture used by HTTP and SCPI formatters.
 */
#include "runtime_state.h"
#include "utility_api.h"
#include "core_api.h"

static void copyText(char* destination, size_t destinationLength, const char* source) {
  if (destinationLength == 0) return;
  snprintf(destination, destinationLength, "%s", source ? source : "");
}

bool captureRuntimeStateSnapshot(RuntimeStateSnapshot& out) {
  memset(&out, 0, sizeof(out));

  CoreOutputSnapshot coreSnapshot{};
  out.outputSnapshotValid = readCoreOutputSnapshot(coreSnapshot);
  if (out.outputSnapshotValid) {
    out.outputSnapshotSequence = coreSnapshot.snapshotSequence;
    out.outputLastCommandSequence = coreSnapshot.lastCommandSequence;
    out.outputSafetyGeneration = coreSnapshot.safetyGeneration;
    out.outputFlags = coreSnapshot.flags;
    for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
      out.masks[ch] = coreSnapshot.masks[ch];
      out.applyCounters[ch] = coreSnapshot.applyCounter[ch];
    }
  } else {
    refreshCore0OutputMirror();
    for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch) {
      out.masks[ch] = channelMask[ch];
      out.applyCounters[ch] = applyCounter[ch];
    }
  }

  copyText(out.status, sizeof(out.status), statusText);
  copyText(out.error, sizeof(out.error), lastError);
  copyText(out.serial, sizeof(out.serial), deviceSerialNumber.c_str());
  copyText(out.firmwareUpdateStatus, sizeof(out.firmwareUpdateStatus), firmwareUpdateStatus);
  const IPAddress activeIp = eth.localIP();
  snprintf(out.ip, sizeof(out.ip), "%u.%u.%u.%u",
           unsigned(activeIp[0]), unsigned(activeIp[1]),
           unsigned(activeIp[2]), unsigned(activeIp[3]));

#ifdef ERESISTOR_TEST_MODE
  out.testMode = true;
#else
  out.testMode = false;
#endif
  out.littleFsReady = littleFsReady;
  out.calibrationSavedMask = calibrationSavedMask;
  out.calibrationLoadedMask = calibrationLoadedMask;
  out.calibrationLoadErrorMask = calibrationLoadErrorMask;
  out.w5500Version = w5500Version;
  out.ethernetFault = ethernetFault;
  out.ethernetInterfaceStarted = ethernetInterfaceStarted;
  out.ethernetServicesStarted = ethernetServicesStarted;
  out.ethernetLinkUp = ethernetLinkUp;
  out.ethernetRecoveryState = ethernetRecoveryState;
  out.ethernetRecoveryAttemptCount = ethernetRecoveryAttemptCount;
  out.ethernetRecoverySuccessCount = ethernetRecoverySuccessCount;
  out.ethernetLinkDownCount = ethernetLinkDownCount;
  out.shiftRegistersReady = shiftRegistersReady;
  out.outputsKnownSafe = outputsKnownSafe;
  out.fatalSafeStateActive = fatalSafeStateActive;
  out.core1EngineReady = core1EngineReady;
  out.core1OutputsReady = core1OutputsReady;
  out.core1Fault = core1Fault;
  const uint32_t nowMs = millis();
  out.ethernetLastTransitionAgeMs = nowMs - ethernetLastTransitionMs;
  out.core1HeartbeatMs = uint32_t(core1HeartbeatMs);
  out.core1HeartbeatAgeMs = nowMs - out.core1HeartbeatMs;
  out.core1LoopCounter = uint32_t(core1LoopCounter);
  out.core1CommandCounter = uint32_t(core1CommandCounter);
  out.core1QueueOverflowCounter = uint32_t(core1QueueOverflowCounter);
  out.core1LoopMaxUs = uint32_t(core1LoopMaxUs);
  out.core1MinFreeStackBytes = uint32_t(core1MinFreeStackBytes);
  out.core1EventCounter = uint32_t(core1EventCounter);
  out.core1EventDropCounter = uint32_t(core1EventDropCounter);
  out.heapTotalBytes = getHeapTotalBytes();
  out.heapUsedBytes = getHeapUsedBytes();
  out.heapFreeBytes = getHeapFreeBytes();
  out.heapUsedPercent = getHeapUsedPercent();
  out.core0LoadPercent = runtimeCore0LoadPct;
  out.loopsPerSecond = runtimeLoopsPerSecond;
  out.runtimeLoopMaxUs = runtimeLoopMaxUs;
  out.httpRequestCount = httpRequestCount;
  out.scpiCommandCount = scpiCommandCount;
  out.uptimeMs = nowMs - bootMillis;
  out.targetCandidates = targetSearchLastCandidates;
  out.targetElapsedUs = targetSearchLastElapsedUs;
  out.targetTimeouts = targetSearchTimeoutCount;
  out.targetCancels = targetSearchCancelCount;
  out.core1StartupStage = coreTransportGetCore1StartupStage();
  out.core1ReadyToken = coreTransportGetCore1ReadyToken();
  out.streamedResponseCount = httpStreamedResponseCount;
  out.calibrationPageLastTempBytes = httpCalibrationPageLastTempBytes;
  out.calibrationPagePeakTempBytes = httpCalibrationPagePeakTempBytes;
  out.stateLastTempBytes = httpStateLastTempBytes;
  out.statePeakTempBytes = httpStatePeakTempBytes;
  out.methodRejectedCount = httpMethodRejectedCount;
  out.apiV1RequestCount = httpApiV1RequestCount;
  getCoreTransportDiagnostics(out.transport);
  return out.outputSnapshotValid;
}
