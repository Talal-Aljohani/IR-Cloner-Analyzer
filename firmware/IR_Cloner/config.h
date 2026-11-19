// IR Remote Cloner & Analyzer — all pins and tunable constants in one place.
#pragma once
#include <Arduino.h>

// ---------------- Pins: lcdwiki 2.8" ESP32-32E display module E32R28T (CYD), see its schematic ----------------
constexpr int PIN_IR_TX = 27;      // SPI connector pin 4 (IO27) -> IR board LINK2 -> R2 -> 2N2222 base (idle LOW!)
constexpr int PIN_IR_RX = 35;      // Expand connector pin 2 (IO35) <- IR board LINK4, ADC1_CH7 (input only)

constexpr int PIN_TFT_CS   = 15;   // ILI9341 on the default SPI bus (VSPI), traces on the board
constexpr int PIN_TFT_RST  = -1;   // tied to the ESP32 EN pin: software reset instead
constexpr int PIN_TFT_DC   = 2;
constexpr int PIN_TFT_MOSI = 13;
constexpr int PIN_TFT_SCLK = 14;
constexpr int PIN_TFT_MISO = 12;
constexpr int PIN_TFT_BL   = 21;   // backlight, HIGH = on

constexpr int PIN_T_CLK = 25;      // XPT2046 touch on its own bus (HSPI)
constexpr int PIN_T_CS  = 33;
constexpr int PIN_T_DIN = 32;      // touch MOSI
constexpr int PIN_T_DO  = 39;      // touch MISO (input only)

constexpr int PIN_RGB_R = 22, PIN_RGB_G = 16, PIN_RGB_B = 17;   // on-board RGB LED, LOW = on: kept off
constexpr int PIN_AUDIO_EN = 4;    // speaker amplifier, HIGH = disabled: kept off

// ---------------- Display ----------------
constexpr uint8_t  TFT_ROTATION = 0;          // portrait: 0 or 2. Use 2 if the picture is upside down
constexpr uint32_t TFT_SPI_HZ   = 20000000;   // traces on the board, no jumper wires any more
constexpr bool     TFT_INVERT   = false;      // set true if the colours look like a photo negative

// ---------------- Receiver (ADC + software demodulation) ----------------
constexpr uint32_t DECODE_FS      = 200000;   // ADC rate used to capture remotes (Sa/s)
constexpr bool     ADC_SWAP_PAIRS = false;    // set true if the spectrum shows a mirror peak at fs/2 - fc
constexpr uint32_t WINDOW_US      = 50;       // envelope window, must be longer than one carrier period (26 us)
constexpr int      GLITCH_WINDOWS = 1;        // a level change must last 1 window (50 us): short spaces survive
constexpr int      MIN_DELTA      = 40;       // ADC counts (~30 mV): smallest signal accepted above ambient
constexpr float    NOISE_K        = 6.0f;     // trigger level = ambient + NOISE_K x ambient noise
constexpr int      LEARN_WINDOWS  = 200;      // 10 ms of ambient light learning before arming
constexpr uint32_t END_GAP_US     = 50000;    // this much silence ends a capture
constexpr uint32_t MAX_CAPTURE_MS = 1500;
constexpr uint16_t MIN_FIRST_MARK_US = 400;   // shorter first pulses are noise, not a remote
constexpr int      MAX_DURATIONS  = 599;      // mark, space, mark ... (always odd, ends with a mark)
constexpr int      RAW_SAMPLES    = 4096;     // raw ADC kept for the waveform + FFT (20 ms at 200 kSa/s)
constexpr int      PRE_SAMPLES    = 256;      // samples kept before the trigger (power of 2)
constexpr int      FFT_MAX_N      = 1024;

// ---------------- Transmitter ----------------
constexpr uint32_t DEFAULT_CARRIER_HZ = 38000;  // used when the carrier could not be measured
constexpr float    CARRIER_DUTY       = 0.33f;  // 1/3 like most remotes

// ---------------- Storage ----------------
constexpr int MAX_REMOTES = 6;     // remotes on the Remotes screen (one row each)
constexpr int MAX_BUTTONS = 12;    // buttons per remote (2 x 6 grid)
constexpr int NAME_LEN    = 12;    // names: up to 11 characters (English letters, digits, - _ . /)

// ---------------- Theme ----------------
constexpr int THEME_ACCENTS = 10;  // accent colours offered on the Theme screen (2 rows of 5)

// ---------------- Battery + backlight ----------------
constexpr int      PIN_BAT_ADC    = 34;       // BAT_ADC: battery / 2 through R2 = R3 = 100k (ADC1_CH6)
constexpr int      BAT_DIVIDER    = 2;
constexpr uint32_t BAT_REFRESH_MS = 30000;    // battery reading in the header
constexpr uint32_t DIM_AFTER_MS   = 30000;    // no touch for 30 s: backlight dimmed
constexpr uint32_t OFF_AFTER_MS   = 180000;   // 3 min: backlight off (a touch wakes it)
constexpr int      DIM_LEVEL      = 20;       // dimmed brightness, 0..255

// ---------------- Keys and scenes ----------------
constexpr uint32_t HOLD_REPEAT_MS = 400;      // key held this long: sent again and again (volume, channel)
constexpr uint32_t REPEAT_GAP_MS  = 40;
constexpr int      MAX_SCENES     = 6;        // a scene sends several saved keys one after the other
constexpr int      MAX_STEPS      = 8;
constexpr uint32_t SCENE_GAP_MS   = 500;      // pause between the keys of a scene

// ---------------- Start screen ----------------
constexpr uint32_t SPLASH_MS = 3000;        // credits shown at start (hold the screen meanwhile = touch calibration)
