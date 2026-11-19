// Battery level (BAT_ADC divider on GPIO34) and display backlight dimming.
#pragma once
#include <Arduino.h>

namespace power {
void begin();                    // backlight on PWM, full brightness
void activity();                 // touch or serial command: full brightness, restarts the dim timer
bool asleep();                   // dimmed or off: the next touch only wakes the screen
void update();                   // call from the main loop: dims after DIM_AFTER_MS, off after OFF_AFTER_MS
// Battery voltage in mV (cached). refresh = take a new reading: the IR ADC stream stops for a few ms.
int batteryMv(bool refresh);
int percent(int mv);             // Li-ion charge estimate from the voltage, 0..100
int batteryPercent();            // smoothed value for the screen: changes only by 2 % or more, -1 = no reading
}
