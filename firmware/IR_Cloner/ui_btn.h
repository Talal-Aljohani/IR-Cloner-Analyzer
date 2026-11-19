// A touch button on the screen. Kept in a header: the Arduino IDE puts its auto-generated
// function prototypes above the sketch code, so types used in signatures must be declared earlier.
#pragma once
#include <Arduino.h>

struct Btn {
  int16_t x, y, w, h;
  const char* label;
};
