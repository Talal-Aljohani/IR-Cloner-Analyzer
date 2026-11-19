/*
 * IR Cloner & Analyzer - the Signals and Systems core (EE 3521)
 * Team: Talal Aljohani, Saad Obaid, Moatasim Ahmad, Yousef Ahmed. Supervisor: Dr. Abdulmajeed Alenezi
 *
 * This file keeps ONLY the signal processing of the device, shortened for reading. The full firmware
 * (screens, storage, touch, drivers) is in IR_Cloner/; each part below names the file it comes from and the
 * chapter of the course (Oppenheim & Willsky, chapters 1-5 and 10) it applies.
 * It is plain C++ with a main() that runs the whole chain on a simulated remote press:
 *     g++ -O2 -o core Signals_Core.cpp      then run ./core
 *
 *  remote key -> 1 NEC code -> 2 x(t) = m(t) c(t) (ch. 1, 3, 4) -> 3 receiver = LTI system, convolution (ch. 2)
 *  -> 4 sampling 200 kSa/s (ch. 1) -> 5 envelope -> 6 threshold -> durations -> 7 decode bits
 *  -> 8 spectrum X(e^jw) at N points (ch. 3, 5): carrier -> 9 RC and H(jw) (ch. 2, 4)
 *  -> 10 difference equation, H(z) (ch. 2, 5, 10) -> 11 aliasing at 50 kSa/s (ch. 1, 5)
 */
#include <cmath>
#include <cstdio>
#include <vector>
using namespace std;

const double PI = 3.14159265358979;
const double FC = 38000;        // carrier (Hz): w0 = 2 pi FC rad/s
const double DUTY = 1.0 / 3;    // carrier duty cycle: 2 T1 = T0 / 3
const double FS = 200000;       // ADC sample rate of the device (Sa/s): Ts = 1 / FS
const double RC = 61e-6;        // receiver time constant (photodiode + R1 + stray C), measured on our board

// ---------------------------------------------------------------- 1. the message: NEC code (ir_decode.cpp)
// Mark = light on, space = light off. Leader 9 ms + 4.5 ms, then 32 bits sent LSB first:
// bit 0 = 562 us mark + 562 us space, bit 1 = 562 us mark + 1687 us space. Bytes: addr, ~addr, cmd, ~cmd.
vector<int> necDurations(int addr, int cmd) {
  const int bytes[4] = {addr, ~addr & 0xFF, cmd, ~cmd & 0xFF};
  vector<int> d = {9000, 4500};
  for (int i = 0; i < 32; ++i) {
    d.push_back(562);
    d.push_back((bytes[i / 8] >> (i % 8)) & 1 ? 1687 : 562);
  }
  d.push_back(562);   // stop mark
  return d;
}

// ---------------------------------------------------------------- 2. x(t) = m(t) c(t) (ir_tx.cpp, RMT carrier)
// Ch. 1: the product of two signals. The message m(t) (1 in a mark) times the periodic carrier c(t), period T0.
// Ch. 3, Example 3.5: c(t) = sum a_k e^(j k w0 t) with a_k = sin(k pi / 3) / (k pi), a_0 = 1/3 (a_3 = 0).
// Ch. 4, frequency shifting: X(jw) = sum a_k M(j(w - k w0)): a copy of the message spectrum at every harmonic.
vector<double> modulate(const vector<int>& d, double fsim) {
  vector<double> x;
  bool mark = true;
  for (int us : d) {
    const int n = (int)lround(us * 1e-6 * fsim);
    for (int i = 0; i < n; ++i) {
      const double phase = fmod(x.size() * FC / fsim, 1.0);
      x.push_back(mark && phase < DUTY ? 1.0 : 0.0);
    }
    mark = !mark;
  }
  x.resize(x.size() + (size_t)(5e-3 * fsim), 0.0);   // 5 ms of darkness after the frame
  return x;
}

// ---------------------------------------------------------------- 3. the receiver as an LTI system (ch. 2)
// Impulse response h(t) = (1/RC) e^(-t/RC) u(t): from RC dy/dt + y = x (root s = -1/RC), h = derivative of the
// step response s(t) = (1 - e^(-t/RC)) u(t). Output y(t) = x(t) * h(t) = integral of x(tau) h(t - tau) d tau,
// written here as a sum; the code uses h[k] x[n - k] because convolution is commutative.
vector<double> convolve(const vector<double>& x, double fsim, double rc) {
  const double T = 1 / fsim;
  vector<double> h((size_t)(6 * rc * fsim));                // 6 RC: the rest of h(t) is < 0.25 %
  for (size_t k = 0; k < h.size(); ++k) h[k] = exp(-(k * T) / rc) / rc;
  vector<double> y(x.size(), 0.0);
  for (size_t n = 0; n < x.size(); ++n)
    for (size_t k = 0; k < h.size() && k <= n; ++k) y[n] += h[k] * x[n - k] * T;
  return y;
}

// ---------------------------------------------------------------- 4. sampling (adc_stream.cpp)
// Ch. 1: x[n] = y(n Ts). Keep one value every fsim/fs points and quantise to 12 bits (0 .. 4095) like the ADC.
// The carrier becomes cos(w0 Ts n) with w0 Ts = 2 pi 38 / 200 = 0.38 pi, inside -pi .. pi (part 11).
vector<int> sample(const vector<double>& y, double fsim, double fs) {
  vector<int> s;
  const double step = fsim / fs;
  for (double i = 0; i < y.size(); i += step) s.push_back((int)lround(y[(size_t)i] * 3000) + 20);
  return s;
}

// ---------------------------------------------------------------- 5 + 6. envelope and threshold (ir_rx.cpp)
// The carrier is removed with 50 us windows (longer than one 26 us carrier period): the envelope is the max of
// each window (a fast, NOT linear step: no h[n] for it; the linear way is part 10). A threshold halfway between
// dark and light, with hysteresis, turns it into marks and spaces, and the time between two edges is a duration.
// (The device also learns the ambient light and noise and follows the signal with a local slicer; left out here.)
vector<int> demodulate(const vector<int>& s, double fs) {
  const int W = (int)lround(fs * 50e-6);
  vector<int> env;
  for (size_t i = 0; i + W <= s.size(); i += W) {
    int m = 0;
    for (int k = 0; k < W; ++k) m = max(m, s[i + k]);
    env.push_back(m);
  }
  int lo = 4095, hi = 0;
  for (int e : env) lo = min(lo, e), hi = max(hi, e);
  const double on = lo + 0.55 * (hi - lo), off = lo + 0.45 * (hi - lo);
  vector<int> d;
  bool mark = false, started = false;
  size_t edge = 0;
  for (size_t w = 0; w < env.size(); ++w) {
    const bool now = mark ? env[w] > off : env[w] > on;
    if (now == mark) continue;
    if (started) d.push_back((int)lround((w - edge) * W * 1e6 / fs));   // time since the last edge
    started = true;
    edge = w;
    mark = now;
  }
  return d;   // mark, space, mark, ... ending with the last mark
}

// ---------------------------------------------------------------- 7. decoding (ir_decode.cpp)
// Uses mark + space PERIODS: the convolution with h(t) stretches each mark and shortens the next space by the
// same amount (part 3), but their sum stays exact. 1.125 ms = bit 0, 2.25 ms = bit 1.
bool near(double v, double target, double tol) { return fabs(v - target) <= tol * target; }

bool decodeNec(const vector<int>& d, int& addr, int& cmd) {
  if (d.size() < 67 || !near(d[0] + d[1], 13500, 0.15)) return false;
  int b[4] = {0, 0, 0, 0};
  for (int i = 0; i < 32; ++i) {
    const int period = d[2 + 2 * i] + d[3 + 2 * i];
    if (near(period, 2250, 0.25)) b[i / 8] |= 1 << (i % 8);
    else if (!near(period, 1125, 0.35)) return false;
  }
  if ((b[2] ^ b[3]) != 0xFF) return false;      // error check: the command comes with its inverse
  addr = b[0];
  cmd = b[2];
  return true;
}

// ---------------------------------------------------------------- 8. carrier from the spectrum (dsp.cpp)
// Ch. 5: X(e^jw) = sum x[n] e^(-j w n), computed at w_k = 2 pi k / N only: X[k] = N a_k, the DTFS analysis
// equation of ch. 3. The FFT below is just a fast way to compute these N sums (a programming method, not in
// the course). Before it: mean (a_0) removed and a Hann window (ch. 5 multiplication property: a product in time
// is a periodic convolution in frequency; the rectangular cut of Example 5.3 has big side lobes, Hann small ones).
void fft(vector<double>& re, vector<double>& im) {
  const int n = (int)re.size();
  for (int i = 1, j = 0; i < n; ++i) {                     // bit-reversal permutation
    int bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) swap(re[i], re[j]), swap(im[i], im[j]);
  }
  for (int len = 2; len <= n; len <<= 1) {                 // butterflies
    const double ang = -2 * PI / len;
    for (int i = 0; i < n; i += len)
      for (int k = 0; k < len / 2; ++k) {
        const double wr = cos(ang * k), wi = sin(ang * k);
        const int a = i + k, b = a + len / 2;
        const double tr = re[b] * wr - im[b] * wi, ti = re[b] * wi + im[b] * wr;
        re[b] = re[a] - tr, im[b] = im[a] - ti;
        re[a] += tr, im[a] += ti;
      }
  }
}

// Strongest |X[k]| between fLo and fHi (f_k = k fs / N), refined between bins by a parabola through 3 bins.
double peakFreq(const vector<int>& s, int start, int n, double fs, double fLo, double fHi) {
  vector<double> re(n), im(n, 0.0), mag(n / 2);
  double mean = 0;
  for (int i = 0; i < n; ++i) mean += s[start + i];
  mean /= n;
  for (int i = 0; i < n; ++i) re[i] = (s[start + i] - mean) * (0.5 - 0.5 * cos(2 * PI * i / (n - 1)));
  fft(re, im);
  for (int k = 0; k < n / 2; ++k) mag[k] = sqrt(re[k] * re[k] + im[k] * im[k]);
  const int k0 = max(1, (int)ceil(fLo * n / fs)), k1 = min(n / 2 - 2, (int)(fHi * n / fs));
  int kp = k0;
  for (int k = k0; k <= k1; ++k) kp = mag[k] > mag[kp] ? k : kp;
  const double a = log(mag[kp - 1]), b = log(mag[kp]), c = log(mag[kp + 1]);
  return (kp + 0.5 * (a - c) / (a - 2 * b + c)) * fs / n;
}

// ---------------------------------------------------------------- 9. RC and H(jw) (app.cpp, Math)
// After a mark the output decays as e^(-t/RC) (part 3): from 90 % to 10 % it takes RC ln 9.
// Ch. 4, Example 4.1: e^(-at) u(t) <-> 1 / (a + jw), so H(jw) = 1 / (1 + j w RC), |H(jw)| = 1 / sqrt(1 + (w RC)^2).
double measureRC(const vector<int>& s, double fs, int markEndGuess) {
  const int P = (int)ceil(fs / 36000);                     // max over one carrier period: ripple is not a fall
  auto level = [&](int i) { int m = 0; for (int k = 0; k < P; ++k) m = max(m, s[i + k]); return m; };
  const int base = s[s.size() - 10], top = level(markEndGuess - 20 * P);
  int i90 = -1, i10 = -1;
  for (int i = markEndGuess - 10 * P; i + P < (int)s.size(); ++i) {
    if (i90 < 0 && level(i) < base + 0.9 * (top - base)) i90 = i;
    if (i90 >= 0 && level(i) < base + 0.1 * (top - base)) { i10 = i; break; }
  }
  return (i10 - i90) / fs / log(9.0);
}

// ---------------------------------------------------------------- 10. a digital low-pass filter (power.cpp, Math)
// Ch. 2: recursive difference equation y[n] = a y[n-1] + (1 - a) x[n] (IIR), h[n] = (1 - a) a^n u[n].
// Ch. 10, Example 10.1 + time shifting: H(z) = (1 - a) / (1 - a z^-1), zero z = 0, pole z = a, ROC |z| > a.
// Causal (ROC outside the pole) and stable (the ROC contains |z| = 1 because a < 1).
// a = e^(-Ts / 50 us): h[n] is the sampled e^(-t / 50 us) (ch. 1: C alpha^n with alpha = e^beta).
// Ch. 5, Example 5.1 (z = e^jw): |H(e^jw)| = (1 - a) / sqrt(1 - 2 a cos w + a^2).
// (The battery reading uses the same filter with a = 0.75.)
vector<double> iir(const vector<int>& x, double a) {
  vector<double> y(x.size());
  double prev = x.empty() ? 0 : x[0];
  for (size_t n = 0; n < x.size(); ++n) y[n] = prev = a * prev + (1 - a) * x[n];
  return y;
}

// ---------------------------------------------------------------- 11. aliasing (dsp.cpp, Sampling screen)
// Ch. 1: e^(j(w + 2 pi)n) = e^(jwn), so the DT frequency w0 Ts = 2 pi f / fs only matters modulo 2 pi, and
// cos is even: fold it into 0 .. pi and turn it back into Hz. 38 kHz at 50 kSa/s: 1.52 pi -> 0.48 pi = 12 kHz.
double aliasOf(double f, double fs) {
  const double r = fmod(f, fs);
  return min(r, fs - r);
}

int main() {
  const double FSIM = 2e6;                                  // "continuous" time: 2 MHz simulation grid
  const vector<int> sent = necDurations(0x04, 0x08);        // LG TV power key
  const vector<double> light = modulate(sent, FSIM);
  const vector<double> volt = convolve(light, FSIM, RC);
  const vector<int> s = sample(volt, FSIM, FS);
  printf("1-4  NEC 0x04/0x08, %zu durations -> %.0f ms of light -> %zu ADC samples at %.0f kSa/s\n",
         sent.size(), light.size() / FSIM * 1e3, s.size(), FS / 1000);

  const vector<int> d = demodulate(s, FS);
  printf("5-6  measured durations (us):");
  for (size_t i = 0; i < 8 && i < d.size(); ++i) printf(" %d", d[i]);
  printf(" ...  (sent 9000 4500 562 562 ...: marks longer, spaces shorter, as the convolution predicts)\n");

  int addr = 0, cmd = 0;
  if (decodeNec(d, addr, cmd)) printf("7    decoded NEC address 0x%02X command 0x%02X\n", addr, cmd);
  else printf("7    decode failed\n");

  const int start = (int)(0.2e-3 * FS);                     // 0.2 ms inside the 9 ms leader mark
  const double f = peakFreq(s, start, 1024, FS, 25000, 60000);
  printf("8    N=1024: carrier %.2f kHz (w0 Ts = %.2f pi), bins every fs/N = %.0f Hz\n", f / 1000, 2 * f / FS, FS / 1024);

  const double rc = measureRC(s, FS, (int)(9e-3 * FS));
  const double wrc = 2 * PI * FC * rc;
  printf("9    RC %.1f us (model %.0f): root s = -1/RC = %.0f 1/s, |H(jw)| at 38 kHz = %.3f\n",
         rc * 1e6, RC * 1e6, -1 / rc, 1 / sqrt(1 + wrc * wrc));

  const double a = exp(-1 / (FS * 50e-6));
  const double w = 2 * PI * FC / FS;
  const vector<double> y = iir(s, a);
  double ripIn = 0, ripOut = 0;                             // carrier ripple inside the leader, before / after
  for (int i = start, lo = 4095, hi = 0, lo2 = 1e9, hi2 = 0; i < start + 200; ++i) {
    lo = min(lo, s[i]), hi = max(hi, s[i]), ripIn = hi - lo;
    lo2 = min(lo2, (int)y[i]), hi2 = max(hi2, (int)y[i]), ripOut = hi2 - lo2;
  }
  printf("10   H(z) pole z = a = %.3f, |H(e^jw)| at w0 Ts = %.3f: carrier ripple %.0f -> %.0f ADC counts\n", a,
         (1 - a) / sqrt(1 - 2 * a * cos(w) + a * a), ripIn, ripOut);

  const vector<int> s50 = sample(volt, FSIM, 50000);
  const double f50 = peakFreq(s50, (int)(0.2e-3 * 50000), 256, 50000, 2000, 25000);
  printf("11   at 50 kSa/s w0 Ts = %.2f pi > pi: the carrier shows at %.1f kHz, theory 0.48 pi = %.1f kHz (aliasing)\n",
         2 * FC / 50000, f50 / 1000, aliasOf(FC, 50000) / 1000);
  return 0;
}
