/*
 * IR Remote Cloner & Analyzer — Signals and Systems (EE 3521) project, IU Madinah
 * Team: Talal Aljohani, Saad Obaid, Moatasim Ahmad, Yousef Ahmed
 * Supervisor: Dr. Abdulmajeed Alenezi
 *
 * Hardware: lcdwiki 2.8" ESP32-32E display module (E32R28T, CYD). Arduino board: ESP32 Dev Module, core 3.x
 * Libraries (Library Manager): Adafruit ILI9341, Adafruit GFX, Adafruit BusIO
 * FFT, touch driver and IR decoding are written in this sketch (no other libraries).
 *
 * Signal chain:  photodiode -> ADC 200 kSa/s (DMA) -> 50 us window level + carrier ripple (software demodulation)
 *                -> mark/space timings -> NEC / pulse-distance decoder -> save in flash
 *                -> replay with RMT + 38 kHz hardware carrier on GPIO27.
 *                Raw samples of the first mark -> Hann window + FFT -> carrier frequency.
 *
 * Serial (115200): c = capture, s = send, r = dump raw ADC, x = dump experiment, k = touch calibration, h = help
 */
// All the code is in the .cpp files (app.cpp = screens); the .ino stays minimal on purpose:
// the Arduino IDE rewrites only .ino files (auto prototypes), never .cpp files.
#include "app.h"

void setup() { appSetup(); }

void loop() { appLoop(); }
