#include "dsp.h"
#include "config.h"

namespace {
float re[FFT_MAX_N];
float im[FFT_MAX_N];
}

namespace dsp {

void fft(float* xr, float* xi, int n) {
  // Bit-reversal permutation.
  for (int i = 1, j = 0; i < n; ++i) {
    int bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) {
      std::swap(xr[i], xr[j]);
      std::swap(xi[i], xi[j]);
    }
  }
  // Butterflies.
  for (int len = 2; len <= n; len <<= 1) {
    const float ang = -2.0f * PI / len;
    const float wr = cosf(ang), wi = sinf(ang);
    for (int i = 0; i < n; i += len) {
      float cr = 1, ci = 0;
      for (int k = 0; k < len / 2; ++k) {
        const int a = i + k, b = a + len / 2;
        const float tr = xr[b] * cr - xi[b] * ci;
        const float ti = xr[b] * ci + xi[b] * cr;
        xr[b] = xr[a] - tr;
        xi[b] = xi[a] - ti;
        xr[a] += tr;
        xi[a] += ti;
        const float ncr = cr * wr - ci * wi;
        ci = cr * wi + ci * wr;
        cr = ncr;
      }
    }
  }
}

float* spectrum(const uint16_t* x, int n) {
  n = min(n, FFT_MAX_N);
  float mean = 0;
  for (int i = 0; i < n; ++i) mean += x[i];
  mean /= n;
  for (int i = 0; i < n; ++i) {
    const float w = 0.5f - 0.5f * cosf(2.0f * PI * i / (n - 1));   // Hann window
    re[i] = (x[i] - mean) * w;
    im[i] = 0;
  }
  fft(re, im, n);
  // Amplitude of a sine in ADC counts: 2|X|/N, divided by the Hann coherent gain 0.5.
  for (int k = 0; k < n / 2; ++k) re[k] = 4.0f * sqrtf(re[k] * re[k] + im[k] * im[k]) / n;
  return re;
}

float peak(const float* mag, int n, float fs, float fLo, float fHi, float* snrDb) {
  const int kLo = max(1, (int)ceilf(fLo * n / fs));
  const int kHi = min(n / 2 - 2, (int)floorf(fHi * n / fs));
  if (kHi <= kLo) {
    if (snrDb) *snrDb = 0;
    return 0;
  }
  int kMax = kLo;
  float sum = 0;
  for (int k = kLo; k <= kHi; ++k) {
    sum += mag[k];
    if (mag[k] > mag[kMax]) kMax = k;
  }
  const float avg = sum / (kHi - kLo + 1);
  if (snrDb) *snrDb = avg > 0 ? 20.0f * log10f(mag[kMax] / avg) : 0;

  // Parabolic interpolation on the log magnitude of the 3 bins around the peak.
  float delta = 0;
  if (kMax > 0 && mag[kMax - 1] > 0 && mag[kMax] > 0 && mag[kMax + 1] > 0) {
    const float a = logf(mag[kMax - 1]), b = logf(mag[kMax]), c = logf(mag[kMax + 1]);
    const float den = a - 2 * b + c;
    if (den != 0) delta = constrain(0.5f * (a - c) / den, -0.5f, 0.5f);
  }
  return (kMax + delta) * fs / n;
}

float aliasOf(float f, float fs) {
  const float r = fmodf(f, fs);
  return min(r, fs - r);
}

}  // namespace dsp
