// IR transmitter: replays mark/space timings on GPIO27 with a hardware 38 kHz carrier (RMT peripheral).
#pragma once
#include "ir_types.h"

namespace irtx {
bool begin();                     // call first thing in setup(): keeps the LEDs OFF
bool send(const IrSignal& sig);   // blocking, returns when the whole command is sent
// Non-blocking: LOW for leadUs, then the signal. Used by the loop-back self-test (capture runs meanwhile).
bool sendAsync(const IrSignal& sig, uint32_t leadUs);
bool busy();
}
