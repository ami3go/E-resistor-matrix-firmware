/**
 * @file app.h
 * @brief Compatibility umbrella for the Gate 5 focused firmware interfaces.
 *
 * New modules should include the smallest focused header they require.  This
 * umbrella remains for legacy modules and Arduino sketch compatibility only.
 */
#pragma once
#include "platform.h"
#include "firmware_identity.h"
#include "firmware_types.h"
#include "runtime_state.h"
#include "utility_api.h"
#include "calibration_api.h"
#include "core_api.h"
#include "http_api.h"
#include "scpi_api.h"
#include "firmware_entry.h"
