#include "ir_rx.h"
#include "adc_stream.h"
#include "dsp.h"

namespace {
uint16_t rawBuf[RAW_SAMPLES];
int rawLen = 0;
int rawTrig = 0;
float rawFsHz = 0;
uint16_t ring[PRE_SAMPLES];
uint16_t chunk[512];

static_assert((PRE_SAMPLES & (PRE_SAMPLES - 1)) == 0, "PRE_SAMPLES must be a power of 2");
static_assert(MAX_DURATIONS % 2 == 1, "MAX_DURATIONS must be odd");
}  // namespace

namespace irrx {

/*
 * Demodulation (the "Path B" part of the project). Samples are cut into 50 us windows
 * (10 samples at 200 kSa/s, longer than one 26 us carrier period). Each window gives two numbers:
 *   level  = max of the window      -> light present (works even if R1*C filters the carrier out)
 *   ripple = max - min of the window -> 38 kHz carrier present (immune to ambient light and 120 Hz lamp flicker)
 * Both are measured against their own ambient baseline and noise, learned while nothing is sent.
 * Each one is then scaled by its own peak inside the signal, and the larger of the two decides
 * mark / space, with hysteresis (0.5 to enter a mark, 0.3 to leave it) and a 2-window glitch filter.
 * The time between edges gives the mark/space durations.
 */
Result capture(IrSignal& sig, uint32_t waitMs, CancelFn cancel) {
  if (!adcStream::start(DECODE_FS)) return Result::ADC_ERROR;
  const float fs = adcStream::measuredFs();
  const int W = max(2, (int)lroundf(fs * WINDOW_US * 1e-6f));     // samples per window
  const float winUs = W * 1e6f / fs;                              // real window length
  const uint32_t endGapWin = (uint32_t)(END_GAP_US / winUs);
  const uint32_t maxWin = (uint32_t)(MAX_CAPTURE_MS * 1000.0f / winUs);

  bool learnedFirst = false;
  float baseL = 0, baseR = 0, noiseL = 4, noiseR = 4;   // ambient level / ripple and their noise
  float peakL = 0, peakR = 0;                            // strongest level / ripple seen in this signal
  float sliceHi = 1, sliceLo = 0;                        // local max / min of the normalised signal (data slicer)
  int learned = 0, trigCount = 0, candCount = 0;
  bool triggered = false, mark = false;
  uint32_t wi = 0, trigWin = 0, lastEdge = 0, candStart = 0, sc = 0;
  int wmax = 0, wmin = 4095, wfill = 0;

  sig.n = 0;
  sig.carrierHz = 0;
  rawLen = 0;
  rawFsHz = fs;
  const uint32_t t0 = millis();
  uint32_t lastCancel = t0;

  // Called for every finished window. Returns true when the capture is complete.
  auto onWindow = [&](int level, int ripple) -> bool {
    if (!learnedFirst) {
      baseL = level;
      baseR = ripple;
      learnedFirst = true;
      return false;
    }
    const float dl = level - baseL, dr = ripple - baseR;
    const float sl = dl / max(noiseL, 1.0f), sr = dr / max(noiseR, 1.0f);   // in noise units

    if (!triggered) {
      if (learned > LEARN_WINDOWS && max(sl, sr) > NOISE_K && max(dl, dr) > MIN_DELTA) {
        if (++trigCount < 2) return false;
        // Signal confirmed: it started one window ago.
        triggered = true;
        mark = true;
        peakL = max(sl, NOISE_K);
        peakR = max(sr, NOISE_K);
        sliceHi = 1; sliceLo = 0;
        trigWin = wi - 1;
        lastEdge = trigWin;
        candCount = 0;
        const uint32_t pre = min<uint32_t>(sc, PRE_SAMPLES);
        for (uint32_t k = 0; k < pre; ++k) rawBuf[k] = ring[(sc - pre + k) & (PRE_SAMPLES - 1)];
        rawLen = pre;
        rawTrig = max(0, (int)pre - 2 * W);
        return false;
      }
      trigCount = 0;
      baseL += dl / 32;                    // slowly follow ambient light (tau ~ 1.6 ms)
      noiseL += (fabsf(dl) - noiseL) / 32;
      baseR += dr / 32;
      noiseR += (fabsf(dr) - noiseR) / 32;
      ++learned;
      return false;
    }

    if (mark) {
      peakL = max(peakL, sl);
      peakR = max(peakR, sr);
    }
    const float rel = max(sl / peakL, sr / peakR);       // 1 = as strong as the strongest part of the signal
    // Data slicer: compare with the LOCAL max/min (they relax over ~1.5 ms), not with the strongest pulse.
    // A slow sensor never reaches the leader's level on the short bits; this still sees each rise and fall.
    sliceHi = max(rel, sliceHi - (sliceHi - sliceLo) / 30.0f);
    sliceLo = min(rel, sliceLo + (sliceHi - sliceLo) / 30.0f);
    const float amp = sliceHi - sliceLo, mid = (sliceHi + sliceLo) / 2;
    bool on = mark;                                      // too little swing: keep the current state
    if (amp >= 0.15f) on = mark ? (rel >= mid - 0.1f * amp) : (rel >= mid + 0.1f * amp);

    if (on != mark) {
      if (candCount == 0) candStart = wi;
      if (++candCount >= GLITCH_WINDOWS) {
        const uint32_t d = lroundf((candStart - lastEdge) * winUs);
        sig.us[sig.n++] = (uint16_t)min<uint32_t>(d, 65535);
        lastEdge = candStart;
        mark = on;
        candCount = 0;
        if (!mark && sig.n >= MAX_DURATIONS - 2) return true;   // buffer full (ends on a mark)
      }
    } else {
      candCount = 0;
      if (!mark) {                         // keep following the ambient light between marks
        baseL += dl / 16;
        baseR += dr / 16;
      }
    }
    if (!mark && wi - lastEdge >= endGapWin) return true;      // long silence: end of command
    if (wi - trigWin > maxWin) {                                // too long: close the last mark
      if (mark) sig.us[sig.n++] = (uint16_t)min<uint32_t>(lroundf((wi - lastEdge) * winUs), 65535);
      return true;
    }
    return false;
  };

  adcStream::flush();
  while (true) {
    if (!triggered) {
      const uint32_t now = millis();
      if (now - t0 > waitMs) return Result::TIMEOUT;
      if (cancel && now - lastCancel > 50) {
        lastCancel = now;
        if (cancel()) return Result::CANCELLED;
      }
    }
    const size_t got = adcStream::read(chunk, 512, 50);
    for (size_t i = 0; i < got; ++i) {
      const uint16_t s = chunk[i];
      if (!triggered) ring[sc & (PRE_SAMPLES - 1)] = s;
      else if (rawLen < RAW_SAMPLES) rawBuf[rawLen++] = s;
      ++sc;
      if (s > wmax) wmax = s;
      if (s < wmin) wmin = s;
      if (++wfill < W) continue;
      const bool done = onWindow(wmax, wmax - wmin);
      wmax = 0;
      wmin = 4095;
      wfill = 0;
      ++wi;
      if (!done) continue;
      // Every common protocol starts with a long first mark (NEC 9 ms, Sony 2.4 ms, RC5 0.9 ms).
      if (sig.n >= 1 && sig.us[0] > 20000) {
        // No remote sends a 20 ms mark: the photodiode node did not fall back between bursts.
        Serial.printf("rejected: first mark %u us (> 20 ms). The receiver stays HIGH between pulses: R1 too big "
                      "(try 100k), strong light, or the sensor is a slow phototransistor.\n", sig.us[0]);
      } else if (sig.n >= 3 && sig.us[0] >= MIN_FIRST_MARK_US) {
        return Result::OK;
      }
      // Too short to be a remote: report it (helps to find the noise source) and keep waiting.
      Serial.printf("ignored noise: %u edges, first mark %u us, level %.0f/%.0f ripple %.0f/%.0f (base/noise)\n",
                    sig.n, sig.n ? sig.us[0] : 0, baseL, noiseL, baseR, noiseR);
      triggered = false;
      sig.n = 0;
      rawLen = 0;
      trigCount = 0;
    }
  }
}

Result captureRaw(uint32_t fs, uint16_t* out, int n, uint32_t waitMs, CancelFn cancel) {
  if (!adcStream::start(fs)) return Result::ADC_ERROR;
  const float fsr = adcStream::measuredFs();
  const int learnN = max(256, (int)(fsr * 0.02f));     // 20 ms of ambient light
  const int skipN = (int)(fsr * 200e-6f);               // start 0.2 ms inside the first mark

  double sum = 0, absDev = 0;
  int learnedN = 0;
  float base = 0, noise = 0;
  int phase = 0;                                         // 0 learn mean, 1 learn noise, 2 wait, 3 skip, 4 record
  int count = 0, pos = 0;
  const uint32_t t0 = millis();
  uint32_t lastCancel = t0;

  adcStream::flush();
  while (true) {
    if (phase < 3) {
      const uint32_t now = millis();
      if (now - t0 > waitMs) return Result::TIMEOUT;
      if (cancel && now - lastCancel > 50) {
        lastCancel = now;
        if (cancel()) return Result::CANCELLED;
      }
    }
    const size_t got = adcStream::read(chunk, 512, 50);
    for (size_t i = 0; i < got; ++i) {
      const uint16_t s = chunk[i];
      switch (phase) {
        case 0:
          sum += s;
          if (++learnedN == learnN) { base = sum / learnN; learnedN = 0; phase = 1; }
          break;
        case 1:
          absDev += fabsf(s - base);
          if (++learnedN == learnN) { noise = absDev / learnN; phase = 2; }
          break;
        case 2:
          if (s > base + max(NOISE_K * noise, (float)MIN_DELTA)) { phase = 3; count = 0; }
          break;
        case 3:
          if (++count >= skipN) phase = 4;
          break;
        default:
          out[pos++] = s;
          if (pos >= n) {
            rawFsHz = fsr;
            return Result::OK;
          }
      }
    }
  }
}

const uint16_t* raw() { return rawBuf; }
int rawCount() { return rawLen; }
int rawTrigger() { return rawTrig; }
float rawFs() { return rawFsHz; }

float estimateCarrier(float* snrDb) {
  if (snrDb) *snrDb = 0;
  const int start = rawTrig + (int)(rawFsHz * 200e-6f);
  int n = FFT_MAX_N;
  while (n >= 128 && start + n > rawLen) n /= 2;
  if (n < 128) return 0;
  float* mag = dsp::spectrum(rawBuf + start, n);
  const float fHi = min(60000.0f, rawFsHz * 0.5f);
  float snr = 0;
  const float f = dsp::peak(mag, n, rawFsHz, 25000.0f, fHi, &snr);
  if (snrDb) *snrDb = snr;
  return snr >= 8.0f ? f : 0;
}

}  // namespace irrx
