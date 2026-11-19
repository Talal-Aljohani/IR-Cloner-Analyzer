// Shared data types.
#pragma once
#include "config.h"

// One captured remote command, stored as raw timings (works for any protocol).
struct IrSignal {
  uint32_t carrierHz = 0;          // measured carrier, 0 = unknown (send with DEFAULT_CARRIER_HZ)
  uint16_t n = 0;                  // number of durations: mark, space, mark, ... (odd)
  uint16_t us[MAX_DURATIONS];      // durations in microseconds
};

// What the decoder understood from a signal.
struct IrDecoded {
  enum Proto : uint8_t { UNKNOWN, NEC, NEC_EXT, NEC_REPEAT } proto = UNKNOWN;
  uint16_t address = 0;
  uint8_t command = 0;
  uint16_t bits = 0;               // number of data bits
  uint8_t data[32] = {};           // data bits, LSB first in each byte (up to 256 bits)
  char line1[40] = "";             // short text for the screen
  char line2[40] = "";
};
