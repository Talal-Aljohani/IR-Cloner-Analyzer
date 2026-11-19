#include "touch_xpt2046.h"
#include "config.h"
#include <SPI.h>

namespace {
SPIClass touchSpi(HSPI);
TouchCal calib;
int lastZ1 = 0, lastZ2 = 0, lastX = 0, lastY = 0;
constexpr int Z_THRESHOLD = 400;

// Average of the two closest readings out of three (rejects one noisy sample).
int bestTwo(int a, int b, int c) {
  const int ab = abs(a - b), ac = abs(a - c), bc = abs(b - c);
  if (ab <= ac && ab <= bc) return (a + b) / 2;
  if (ac <= ab && ac <= bc) return (a + c) / 2;
  return (b + c) / 2;
}
}  // namespace

namespace touch {

void begin() {
  pinMode(PIN_T_CS, OUTPUT);
  digitalWrite(PIN_T_CS, HIGH);
  touchSpi.begin(PIN_T_CLK, PIN_T_DO, PIN_T_DIN, -1);
}

bool readRaw(int& rx, int& ry) {
  // XPT2046 control bytes: 0xB1 = Z1, 0xC1 = Z2, 0x91 = Y, 0xD1 = X, 0xD0 = X + power down.
  touchSpi.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
  digitalWrite(PIN_T_CS, LOW);
  touchSpi.transfer(0xB1);
  const int z1 = touchSpi.transfer16(0xC1) >> 3;
  const int z2 = touchSpi.transfer16(0x91) >> 3;
  const int z = z1 + 4095 - z2;
  lastZ1 = z1;
  lastZ2 = z2;
  int y[3] = {0, 0, 0}, x[3] = {0, 0, 0};
  if (z >= Z_THRESHOLD) {
    touchSpi.transfer16(0x91);                  // first Y reading is noisy: discard
    y[0] = touchSpi.transfer16(0xD1) >> 3;
    x[0] = touchSpi.transfer16(0x91) >> 3;
    y[1] = touchSpi.transfer16(0xD1) >> 3;
    x[1] = touchSpi.transfer16(0x91) >> 3;
  }
  y[2] = touchSpi.transfer16(0xD0) >> 3;
  x[2] = touchSpi.transfer16(0) >> 3;
  digitalWrite(PIN_T_CS, HIGH);
  touchSpi.endTransaction();
  // A press needs a real Z1 reading. A silent chip (T_DO or T_CS not connected) reads all 0 or all 1:
  // z1 = 0 or z1 > 4095, which must not count as a press.
  const bool plausible = z1 > 50 && z1 <= 4095 && z2 <= 4095;
  if (!plausible || z < Z_THRESHOLD) return false;
  rx = bestTwo(x[0], x[1], x[2]);
  ry = bestTwo(y[0], y[1], y[2]);
  lastX = rx;
  lastY = ry;
  return true;
}

bool read(int& x, int& y) {
  int rx, ry;
  if (!readRaw(rx, ry)) return false;
  const int a = calib.swapXY ? ry : rx;
  const int b = calib.swapXY ? rx : ry;
  x = lroundf(calib.ax * a + calib.bx);
  y = lroundf(calib.ay * b + calib.by);
  return true;
}

void setCal(const TouchCal& c) { calib = c; }

void lastRaw(int& z1, int& z2, int& x, int& y) {
  z1 = lastZ1;
  z2 = lastZ2;
  x = lastX;
  y = lastY;
}
const TouchCal& cal() { return calib; }

bool valid(const TouchCal& c) {
  // The 12-bit raw range (~200..3900) maps to 320 x 240 px: |scale| is about 0.06..0.1 px per count.
  auto ok = [](float a) { return isfinite(a) && fabsf(a) > 0.02f && fabsf(a) < 0.5f; };
  return c.magic == CAL_MAGIC && ok(c.ax) && ok(c.ay) && isfinite(c.bx) && isfinite(c.by);
}

TouchCal solve(const int sx[3], const int sy[3], const int rx[3], const int ry[3]) {
  TouchCal c;
  c.magic = CAL_MAGIC;
  // Moving along screen X (P0 -> P1): the raw axis that changed more is the X axis.
  c.swapXY = abs(ry[1] - ry[0]) > abs(rx[1] - rx[0]);
  const float a0 = c.swapXY ? ry[0] : rx[0], a1 = c.swapXY ? ry[1] : rx[1];
  const float b0 = c.swapXY ? rx[0] : ry[0], b2 = c.swapXY ? rx[2] : ry[2];
  c.ax = (a1 != a0) ? (sx[1] - sx[0]) / (a1 - a0) : 1;
  c.bx = sx[0] - c.ax * a0;
  c.ay = (b2 != b0) ? (sy[2] - sy[0]) / (b2 - b0) : 1;
  c.by = sy[0] - c.ay * b0;
  return c;
}

}  // namespace touch
