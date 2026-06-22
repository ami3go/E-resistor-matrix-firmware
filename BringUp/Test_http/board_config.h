#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include <Arduino.h>

/*
  Per-bit resistor mapping.

  Important:
    Firmware logic must NOT calculate resistor value from bit number.
    This table is the source of truth for UI/documentation.

  Bit mapping:
    bit 0  -> Q16, lowest resistance branch, 626R
    bit 15 -> Q1, highest resistance branch, 20M

  Replace "TODO" entries with your real BOM values.
*/

struct ResistorBitInfo {
  uint8_t bit;
  const char* branch_name;
  const char* nominal_resistance;
};

static constexpr ResistorBitInfo RESISTOR_BIT_TABLE[16] = {
  {0,  "Q16", "626R"},
  {1,  "Q15", "1.24k"},
  {2,  "Q14", "2.5k"},
  {3,  "Q13", "5k"},
  {4,  "Q12", "10k"},
  {5,  "Q11", "20k"},
  {6,  "Q10", "40.2k"},
  {7,  "Q9",  "80.6"},
  {8,  "Q8",  "160k"},
  {9,  "Q7",  "324k"},
  {10, "Q6",  "643k"},
  {11, "Q5",  "1.27M"},
  {12, "Q4",  "2.49M"},
  {13, "Q3",  "5.1M"},
  {14, "Q2",  "10M"},
  {15, "Q1",  "20M"},
};

#endif