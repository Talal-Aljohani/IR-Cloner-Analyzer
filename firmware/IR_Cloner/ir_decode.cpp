#include "ir_decode.h"
#include <algorithm>

namespace {

// NEC timing (microseconds). One unit = 562.5 us.
constexpr uint16_t NEC_LEAD_MARK = 9000;
constexpr uint16_t NEC_LEAD_SPACE = 4500;
constexpr uint16_t NEC_REPEAT_SPACE = 2250;
constexpr uint16_t NEC_BIT_MARK = 562;
constexpr uint16_t NEC_ONE_SPACE = 1687;
constexpr uint16_t NEC_ZERO_SPACE = 562;

bool near(uint32_t v, uint32_t target, float tol = 0.3f) {
  return v >= target * (1 - tol) && v <= target * (1 + tol);
}

void setBit(IrDecoded& d, int i, bool b) {
  if (b) d.data[i / 8] |= 1 << (i % 8);
}

bool decodeNecAt(const IrSignal& s, int o, IrDecoded& d);

// A capture can start with a short glitch (e.g. "700, 200") before the 9 ms leader: try the first few marks.
bool decodeNec(const IrSignal& s, IrDecoded& d) {
  for (int o = 0; o <= 6 && o + 2 < s.n; o += 2) {
    IrDecoded t;
    if (decodeNecAt(s, o, t)) { d = t; return true; }
  }
  return false;
}

// o = index of the leader mark (even).
bool decodeNecAt(const IrSignal& s, int o, IrDecoded& d) {
  // Decoded from mark + space PERIODS, not from the mark alone: a slow or saturated receiver stretches every
  // mark and shortens the following space by the same amount, but their sum stays exact
  // (leader 13.5 ms, repeat 11.25 ms, bit 0 = 1.125 ms, bit 1 = 2.25 ms).
  const int n = s.n - o;
  if (n < 3 || s.us[o] < 6000) return false;
  const uint32_t lead = (uint32_t)s.us[o] + s.us[o + 1];
  if (n < 67 && near(lead, NEC_LEAD_MARK + NEC_REPEAT_SPACE, 0.15f)) {
    d.proto = IrDecoded::NEC_REPEAT;
    snprintf(d.line1, sizeof d.line1, "NEC repeat code");
    snprintf(d.line2, sizeof d.line2, "(key held down)");
    return true;
  }
  if (n < 67 || !near(lead, NEC_LEAD_MARK + NEC_LEAD_SPACE, 0.15f)) return false;

  for (int i = 0; i < 32; ++i) {
    const uint32_t period = (uint32_t)s.us[o + 2 + 2 * i] + s.us[o + 3 + 2 * i];
    if (near(period, NEC_BIT_MARK + NEC_ONE_SPACE, 0.25f)) setBit(d, i, true);
    else if (!near(period, NEC_BIT_MARK + NEC_ZERO_SPACE, 0.35f)) return false;
  }
  d.bits = 32;
  const uint8_t a = d.data[0], na = d.data[1], c = d.data[2], nc = d.data[3];
  if ((uint8_t)(c ^ nc) != 0xFF) return false;          // command must come with its inverse
  d.command = c;
  if ((uint8_t)(a ^ na) == 0xFF) {
    d.proto = IrDecoded::NEC;
    d.address = a;
    snprintf(d.line1, sizeof d.line1, "NEC  addr 0x%02X  cmd 0x%02X", a, c);
  } else {
    d.proto = IrDecoded::NEC_EXT;
    d.address = a | (na << 8);
    snprintf(d.line1, sizeof d.line1, "NEC-ext  addr 0x%04X  cmd 0x%02X", d.address, c);
  }
  snprintf(d.line2, sizeof d.line2, "raw 0x%02X%02X%02X%02X (LSB first)", d.data[3], d.data[2], d.data[1], d.data[0]);
  return true;
}

}  // namespace

namespace irdecode {

void decode(const IrSignal& sig, IrDecoded& out) {
  out = IrDecoded();
  if (decodeNec(sig, out)) return;
  out = IrDecoded();
  snprintf(out.line1, sizeof out.line1, "Not NEC: timings only");
  snprintf(out.line2, sizeof out.line2, "raw timings saved (%u)", sig.n);
}

void makeNec(uint8_t address, uint8_t command, IrSignal& out) {
  const uint8_t bytes[4] = {address, (uint8_t)~address, command, (uint8_t)~command};
  int n = 0;
  out.us[n++] = NEC_LEAD_MARK;
  out.us[n++] = NEC_LEAD_SPACE;
  for (int i = 0; i < 32; ++i) {
    out.us[n++] = NEC_BIT_MARK;
    out.us[n++] = (bytes[i / 8] >> (i % 8)) & 1 ? NEC_ONE_SPACE : NEC_ZERO_SPACE;
  }
  out.us[n++] = NEC_BIT_MARK;                 // stop mark
  out.n = n;
  out.carrierHz = 38000;
}

namespace {

uint32_t snapCarrier(uint32_t f) {     // nearest standard carrier within 2.5 kHz, else 38 kHz
  const uint32_t std[4] = {36000, 38000, 40000, 56000};
  uint32_t best = 38000, bestErr = 2501;
  for (uint32_t s : std) {
    const uint32_t err = f > s ? f - s : s - f;
    if (err < bestErr) { bestErr = err; best = s; }
  }
  return best;
}

// Replaces every duration of one parity (0 = marks, 1 = spaces) by the median of its cluster.
// Clusters: sorted values split where the next one is > 20 % longer. Returns the median of the shortest cluster.
uint16_t mergeClusters(IrSignal& s, int parity) {
  uint16_t v[MAX_DURATIONS];
  int m = 0;
  for (int i = parity; i < s.n; i += 2) v[m++] = s.us[i];
  if (m < 3) return 0;
  std::sort(v, v + m);
  uint16_t shortest = 0;
  int start = 0;
  for (int i = 1; i <= m; ++i) {
    if (i < m && v[i] <= v[i - 1] * 1.2f) continue;
    const int count = i - start;
    const uint16_t med = v[start + count / 2], lo = v[start], hi = v[i - 1];
    if (start == 0) shortest = count >= 3 ? med : 0;
    if (count >= 2)
      for (int k = parity; k < s.n; k += 2)
        if (s.us[k] >= lo && s.us[k] <= hi) s.us[k] = med;
    start = i;
  }
  return shortest;
}

}  // namespace

void prepareForSend(const IrSignal& in, IrSignal& out) {
  IrDecoded d;
  decode(in, d);
  if (d.proto == IrDecoded::NEC || d.proto == IrDecoded::NEC_EXT) {
    // clean frame from the decoded bytes (keeps extended addresses), then the repeat code 108 ms after the start
    int n = 0;
    out.us[n++] = NEC_LEAD_MARK;
    out.us[n++] = NEC_LEAD_SPACE;
    for (int i = 0; i < 32; ++i) {
      out.us[n++] = NEC_BIT_MARK;
      out.us[n++] = (d.data[i / 8] >> (i % 8)) & 1 ? NEC_ONE_SPACE : NEC_ZERO_SPACE;
    }
    out.us[n++] = NEC_BIT_MARK;
    uint32_t frame = 0;
    for (int i = 0; i < n; ++i) frame += out.us[i];
    out.us[n++] = (uint16_t)(108000 - frame);   // gap to the repeat code
    out.us[n++] = NEC_LEAD_MARK;
    out.us[n++] = NEC_REPEAT_SPACE;
    out.us[n++] = NEC_BIT_MARK;
    out.n = n;
    out.carrierHz = 38000;
    return;
  }
  if (d.proto == IrDecoded::NEC_REPEAT) {
    out.us[0] = NEC_LEAD_MARK; out.us[1] = NEC_REPEAT_SPACE; out.us[2] = NEC_BIT_MARK;
    out.n = 3; out.carrierHz = 38000;
    return;
  }
  // Drop short glitches captured before a long header mark (e.g. "700, 200, 8100, 4500 ...").
  int o = 0;
  while (o < 6 && o + 4 < in.n && in.us[o] < 1500 && in.us[o + 2] >= 3000) o += 2;
  out.n = in.n - o;
  for (int i = 0; i < out.n; ++i) out.us[i] = in.us[o + i];
  out.carrierHz = in.carrierHz;
  const uint16_t m0 = mergeClusters(out, 0), s0 = mergeClusters(out, 1);
  // The receiver stretches marks and shortens spaces by the same amount (envelope window + R1*C decay).
  // Most protocols use equal short mark and short space, so split the difference.
  if (m0 && s0 && m0 > s0) {   // stretch removed even when it is large (saturated, slow sensor)
    const uint16_t b = (m0 - s0) / 2;
    for (int i = 0; i < out.n; ++i) out.us[i] = (i % 2 == 0) ? out.us[i] - min<uint16_t>(b, out.us[i] / 2) : out.us[i] + b;
  }
  out.carrierHz = snapCarrier(in.carrierHz ? in.carrierHz : 38000);
}

}  // namespace irdecode
