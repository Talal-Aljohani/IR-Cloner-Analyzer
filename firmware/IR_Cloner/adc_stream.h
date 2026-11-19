// Continuous (DMA) ADC on GPIO35. analogRead() is far too slow for a 38 kHz carrier.
#pragma once
#include <Arduino.h>

namespace adcStream {
bool start(uint32_t fs);        // (re)configure and start; measures the real rate once
void stop();
void flush();                   // drop samples taken while nobody was reading
uint32_t nominalFs();
float measuredFs();             // real sample rate, measured with esp_timer
// Reads one DMA frame (<= 512 samples) into out. Returns the sample count, 0 on timeout.
size_t read(uint16_t* out, size_t maxSamples, uint32_t timeoutMs);
}
