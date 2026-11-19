// IR receiver: raw photodiode samples -> software demodulation -> mark/space timings.
#pragma once
#include "ir_types.h"

namespace irrx {

enum class Result { OK, TIMEOUT, CANCELLED, ADC_ERROR };

using CancelFn = bool (*)();

// Waits up to waitMs for a remote, records its timings into sig and keeps a raw snippet.
Result capture(IrSignal& sig, uint32_t waitMs, CancelFn cancel);

// Experiment mode: at sample rate fs, waits for light, skips 0.2 ms, stores n raw samples.
Result captureRaw(uint32_t fs, uint16_t* out, int n, uint32_t waitMs, CancelFn cancel);

// Raw snippet of the last capture() (starts PRE_SAMPLES before the first mark).
const uint16_t* raw();
int rawCount();
int rawTrigger();        // index of the first mark inside raw()
float rawFs();           // measured sample rate of raw()

// Carrier frequency from an FFT of the raw snippet; 0 if no clear peak.
float estimateCarrier(float* snrDb);

}  // namespace irrx
