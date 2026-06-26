/**
 * @file rp2040_w5500_resistor_matrix_v1_dual_core_cal_scpi_webhelp_doxygen.ino
 * @brief Minimal Arduino sketch entry file that includes the shared firmware declarations.
 */

/*
  RP2040-Zero + W5500 + 8-channel 74HC595 resistor matrix
  Dual-core modular Arduino sketch using normal .cpp/.h files.

  Core 0: Ethernet, HTTP, SCPI, LittleFS, UI, parsing.
  Core 1: shift-register GPIO, break-before-make switching, safe OFF.

  Pinout is unchanged from the previous safety-logic version.
*/

#include "app.h"
