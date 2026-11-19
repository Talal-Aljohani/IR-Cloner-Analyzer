// Small DSP helpers: radix-2 FFT and spectral peak search (no external library).
#pragma once
#include <Arduino.h>

namespace dsp {

// In-place complex FFT, n must be a power of 2.
void fft(float* re, float* im, int n);

// Removes the mean, applies a Hann window, runs the FFT and returns n/2 amplitudes (ADC counts).
// The returned buffer is internal and stays valid until the next call.
float* spectrum(const uint16_t* x, int n);

// Strongest bin between fLo and fHi, refined by parabolic interpolation.
// snrDb = peak / average of the searched band, in dB.
float peak(const float* mag, int n, float fs, float fLo, float fHi, float* snrDb);

// Expected apparent frequency of a tone f when sampled at fs (aliasing / folding).
float aliasOf(float f, float fs);

}  // namespace dsp
