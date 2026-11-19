// Minimal XPT2046 touch driver on its own SPI bus (HSPI) + 3-point calibration.
#pragma once
#include <Arduino.h>

struct TouchCal {
  uint32_t magic = 0;
  bool swapXY = false;     // true: raw Y moves along the screen X axis
  float ax = 0, bx = 0;    // screenX = ax * rawA + bx
  float ay = 0, by = 0;    // screenY = ay * rawB + by
};

namespace touch {
constexpr uint32_t CAL_MAGIC = 0x54434133;   // "TCA3" (portrait). Changing it forces a new calibration

void begin();
bool readRaw(int& rx, int& ry);              // true while pressed
bool read(int& x, int& y);                   // calibrated screen coordinates
void setCal(const TouchCal& c);
void lastRaw(int& z1, int& z2, int& x, int& y);   // last readings, for the Serial diagnostics
const TouchCal& cal();
bool valid(const TouchCal& c);               // rejects calibrations made from a silent / broken touch chip
// Builds a calibration from the raw readings of 3 screen points:
// P0 (left-top), P1 (right-top: only screen X changes), P2 (left-bottom: only screen Y changes).
TouchCal solve(const int sx[3], const int sy[3], const int rx[3], const int ry[3]);
}
