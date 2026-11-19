// Turns mark/space timings into bits for the NEC protocol (from mark + space periods). Any other remote
// (e.g. an AC) is not decoded: it is cloned from its own timings.
#pragma once
#include "ir_types.h"

namespace irdecode {
void decode(const IrSignal& sig, IrDecoded& out);
// Builds a standard NEC frame (used by the transmitter tests). Carrier 38 kHz.
void makeNec(uint8_t address, uint8_t command, IrSignal& out);
// What is actually transmitted for a captured signal:
//  - NEC: a clean frame with the exact standard timings, plus one repeat code (like a real remote);
//  - other protocols: similar durations merged to their median, and the receiver's mark stretching removed;
//  - carrier snapped to 36/38/40/56 kHz (38 kHz when the measurement is not close to one of them).
void prepareForSend(const IrSignal& in, IrSignal& out);
}
