/**
 * @file core_transport_types.h
 * @brief Allocation-free numeric protocol shared by the two RP2040 cores.
 *
 * This header deliberately contains no Arduino String, networking, filesystem,
 * parser, or human-readable formatting dependency.
 */
#pragma once

#include <stdint.h>
#include "board_config.h"

inline constexpr uint8_t CORE_COMMAND_QUEUE_DEPTH = 8;
inline constexpr uint8_t CORE_RESULT_QUEUE_DEPTH = 8;
inline constexpr uint8_t CORE1_EVENT_QUEUE_DEPTH = 32;
inline constexpr uint32_t CORE_COMMAND_TIMEOUT_MS = 1000UL;
inline constexpr uint32_t CORE0_FAILSAFE_TIMEOUT_MS = 10000UL;
inline constexpr uint32_t CORE_STARTUP_TIMEOUT_MS = 5000UL;
inline constexpr uint8_t CORE_POLICY_SLOT_COUNT = 2;

enum FirmwareLogLevel : uint8_t {
  FW_LOG_ERROR = 0,
  FW_LOG_INFO = 1,
  FW_LOG_DEBUG = 2,
  FW_LOG_TRACE = 3
};

enum CoreCommandType : uint8_t {
  CORE_CMD_NONE = 0,
  CORE_CMD_SET_MASK,
  CORE_CMD_SET_ALL_MASKS,
  CORE_CMD_CLEAR_ALL,
  CORE_CMD_GET_SNAPSHOT,
  CORE_CMD_INSTALL_POLICY
};

enum CoreResponseStatus : int16_t {
  CORE_RESP_OK = 0,
  CORE_RESP_BUSY = -300,
  CORE_RESP_TIMEOUT = -501,
  CORE_RESP_EXPIRED = -502,
  CORE_RESP_GENERATION_MISMATCH = -503,
  CORE_RESP_ENGINE_NOT_READY = -504,
  CORE_RESP_POLICY_NOT_INSTALLED = -505,
  CORE_RESP_INVALID_ARGUMENT = -101,
  CORE_RESP_REJECTED = -222,
  CORE_RESP_IO_ERROR = -500
};

enum CoreResultDetail : uint16_t {
  CORE_DETAIL_NONE = 0,
  CORE_DETAIL_INVALID_CHANNEL = 1,
  CORE_DETAIL_INVALID_COMMAND = 2,
  CORE_DETAIL_ACTIVE_BIT_LIMIT = 3,
  CORE_DETAIL_RESISTANCE_INVALID = 4,
  CORE_DETAIL_RESISTANCE_BELOW_MIN = 5,
  CORE_DETAIL_RESISTANCE_ABOVE_MAX = 6,
  CORE_DETAIL_POLICY_SLOT_INVALID = 7,
  CORE_DETAIL_POLICY_INSTALL_REQUIRES_OFF = 8,
  CORE_DETAIL_SHIFT_REGISTER_NOT_READY = 9,
  CORE_DETAIL_PHYSICAL_APPLY_FAILED = 10,
  CORE_DETAIL_PHYSICAL_ALL_OFF_FAILED = 11,
  CORE_DETAIL_CORE0_HEARTBEAT_EXPIRED = 12,
  CORE_DETAIL_TEST_DELAY_APPLIED = 13,
  CORE_DETAIL_PROFILE_CLEAR_FAILED = 14,
  CORE_DETAIL_PROFILE_MAKE_FAILED = 15,
  CORE_DETAIL_TEST_PROFILE_FAIL_AFTER_CLEAR = 16
};

enum Core1EventCode : uint8_t {
  CORE1_EVT_NONE = 0,
  CORE1_EVT_ENGINE_READY,
  CORE1_EVT_APPLY_BEGIN,
  CORE1_EVT_APPLY_DONE,
  CORE1_EVT_APPLY_REJECTED,
  CORE1_EVT_APPLY_FAILED,
  CORE1_EVT_ALL_OFF_BEGIN,
  CORE1_EVT_ALL_OFF_DONE,
  CORE1_EVT_ALL_OFF_FAILED,
  CORE1_EVT_PROFILE_BEGIN,
  CORE1_EVT_PROFILE_BREAK_BEFORE_MAKE,
  CORE1_EVT_PROFILE_DONE,
  CORE1_EVT_PROFILE_FAILED,
  CORE1_EVT_COMMAND_EXPIRED,
  CORE1_EVT_COMMAND_INVALIDATED,
  CORE1_EVT_POLICY_INSTALLED,
  CORE1_EVT_CORE0_FAILSAFE
};

enum CoreSnapshotFlags : uint16_t {
  CORE_SNAPSHOT_OUTPUTS_SAFE = 1U << 0,
  CORE_SNAPSHOT_ENGINE_READY = 1U << 1,
  CORE_SNAPSHOT_FAULT = 1U << 2,
  CORE_SNAPSHOT_POLICY_INSTALLED = 1U << 3
};

struct Core1Event {
  uint32_t timestampUs;
  uint32_t sequence;
  uint32_t durationUs;
  uint16_t mask;
  uint16_t detail;
  uint8_t code;
  uint8_t level;
  uint8_t channelIndex;
  uint8_t reserved;
};

struct CoreCommand {
  uint32_t sequence;
  uint32_t deadlineAtUs;
  uint32_t safetyGeneration;
  CoreCommandType type;
  uint8_t channelIndex;
  uint8_t policySlot;
  uint8_t reserved;
  uint16_t mask;
  uint16_t masks[CHANNEL_COUNT];
};

struct CoreResult {
  uint32_t sequence;
  uint32_t snapshotSequence;
  uint32_t completedAtUs;
  CoreResponseStatus status;
  uint16_t detail;
  uint8_t channelIndex;
  uint8_t reserved;
};

struct CoreOutputSnapshot {
  uint32_t snapshotSequence;
  uint32_t lastCommandSequence;
  uint32_t safetyGeneration;
  uint32_t applyCounter[CHANNEL_COUNT];
  uint16_t masks[CHANNEL_COUNT];
  uint16_t flags;
  uint16_t reserved;
};

struct CoreSafetySnapshot {
  uint32_t safetyGeneration;
  float resistorOhms[CHANNEL_COUNT][BIT_COUNT];
  float minimumOhms[CHANNEL_COUNT];
  float maximumOhms[CHANNEL_COUNT];
  uint8_t maximumActiveBits[CHANNEL_COUNT];
  uint8_t expertMode;
  uint8_t reserved[3];
};

struct CoreTransportDiagnostics {
  uint32_t commandQueueOverflowCount;
  uint32_t resultQueueOverflowCount;
  uint32_t commandTimeoutCount;
  uint32_t commandExpiredCount;
  uint32_t generationRejectCount;
  uint32_t invalidCommandCount;
  uint32_t policyInstallCount;
  uint32_t core0FailsafeCount;
  uint32_t profileTransitionCount;
  uint32_t profileFailureCount;
  uint32_t profileBreakBeforeMakeCount;
  uint32_t lastProfileDurationUs;
  uint32_t maxProfileDurationUs;
  uint32_t lastProfileClearDurationUs;
  uint32_t maxProfileClearDurationUs;
  uint32_t currentSafetyGeneration;
  uint32_t lastSubmittedSequence;
  uint32_t lastCompletedSequence;
};
