// User interface (portrait 240 x 320), screens and serial commands. Started from IR_Cloner.ino.
#include <Arduino.h>
#include <algorithm>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include "config.h"
#include "ir_types.h"
#include "adc_stream.h"
#include "ir_rx.h"
#include "ir_tx.h"
#include "ir_decode.h"
#include "dsp.h"
#include "touch_xpt2046.h"
#include "storage.h"
#include "ui_btn.h"
#include "app.h"
#include "power.h"

// ---------------- Display + colours ----------------
Adafruit_ILI9341 tft(&SPI, PIN_TFT_DC, PIN_TFT_CS, PIN_TFT_RST);
constexpr int SW = 240, SH = 320;   // portrait (TFT_ROTATION 0 or 2)

constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
// Colours: dark or light mode plus one of 5 accent colours, chosen on the Theme screen and kept in flash.
uint16_t C_BG, C_PANEL, C_EDGE, C_TEXT, C_DIM, C_ACCENT, C_WARN, C_RED, C_OFF;
uint16_t C_INK;            // text on coloured (accent / warning / red) buttons

// dark = bright shade (dark text on it), light = deeper shade (white text on it, readable on the light background)
struct Accent {
  const char* name;
  uint8_t r, g, b, lr, lg, lb;
};
const Accent ACCENTS[THEME_ACCENTS] = {
    {"Teal", 0, 190, 175, 0, 128, 118},    {"Blue", 50, 130, 255, 30, 90, 200},  {"Purple", 160, 100, 245, 120, 70, 200},
    {"Orange", 255, 140, 20, 200, 95, 0},  {"Green", 50, 190, 90, 30, 135, 60},  {"Pink", 245, 90, 170, 200, 40, 120},
    {"Red", 240, 80, 80, 190, 40, 40},     {"Gold", 240, 200, 40, 160, 115, 0},  {"Sky", 60, 195, 245, 0, 120, 180},
    {"Lime", 160, 215, 50, 90, 130, 0}};
bool darkMode = true;
int accentIdx = 0;

uint16_t accentColor(int i) {
  const Accent& a = ACCENTS[i];
  return darkMode ? rgb(a.r, a.g, a.b) : rgb(a.lr, a.lg, a.lb);
}

void applyTheme() {
  if (darkMode) {
    C_BG = rgb(8, 12, 22);
    C_PANEL = rgb(24, 32, 50);
    C_EDGE = rgb(60, 74, 100);
    C_TEXT = rgb(235, 240, 248);
    C_DIM = rgb(140, 152, 175);
    C_WARN = rgb(255, 170, 40);
    C_RED = rgb(230, 70, 70);
    C_OFF = rgb(40, 46, 60);
    C_INK = rgb(8, 12, 22);
  } else {
    C_BG = rgb(236, 240, 246);
    C_PANEL = rgb(255, 255, 255);
    C_EDGE = rgb(170, 180, 198);
    C_TEXT = rgb(18, 24, 36);
    C_DIM = rgb(96, 106, 126);
    C_WARN = rgb(215, 120, 0);
    C_RED = rgb(205, 40, 40);
    C_OFF = rgb(214, 219, 228);
    C_INK = rgb(255, 255, 255);
  }
  C_ACCENT = accentColor(accentIdx);
}

// ---------------- State ----------------
enum class Screen { HOME, RESULT, ANALYZE, STEPS, MATH, REMOTES, REMOTE, KEY, SCENES, SCENE, EXPER, THEME, ABOUT };
Screen screen = Screen::HOME;

IrSignal cur;              // the signal on screen
IrDecoded dec;
bool haveCur = false;
bool rawValid = false;     // raw snippet belongs to cur (false for signals loaded from flash)
char curName[NAME_LEN] = "";   // key name when cur was opened from a remote
float curSnr = 0;

IrSignal keySig;           // a key loaded from flash to send or show
int curRemote = -1;        // remote on the REMOTE screen
bool editMode = false;     // REMOTE screen: a tap on a key opens its menu instead of sending
enum class Pick { NONE, SAVE, STEP };
Pick pick = Pick::NONE;    // REMOTES / REMOTE opened to choose where cur is saved (SAVE) or a key for a scene (STEP)
int menuKey = -1;          // key on the KEY screen
int mathPage = 0;          // MATH screen: 0 = sensor RC (convolution, H(jw)), 1 = Z-transform

const uint32_t EXP_FS[3] = {50000, 200000, 500000};
int expFsIdx = 0;
int expN = 256;
uint16_t expBuf[FFT_MAX_N];
int expLen = 0;
float expFs = 0;

// ---------------- Prototypes ----------------
void showHome();
void showResult();
void showAnalyze();
void showRemotes();
void showRemote();
void showKeyMenu();
void showExper();
void showTheme();
void showAbout();
void drawCredits(bool splash = false);
void showSteps();
void showMath();
void showScenes();
void showScene();
void addSceneStep(int r, int b);
void drawBattery();
void runMonitor(bool fromSerial);
bool runCalibration();
irrx::Result captureInto(IrSignal& dst, bool fromSerial, const char* what, const char* cancelHint, float* snr);
bool captureSignal(bool fromSerial, const char* what);
void doCapture(bool fromSerial);
void doSend(const IrSignal& s, bool log = true);
void printSignal(const IrSignal& s);

// ---------------- Drawing helpers ----------------
void text(int x, int y, const char* s, uint8_t size = 1, uint16_t color = C_TEXT) {
  tft.setTextSize(size);
  tft.setTextColor(color);
  tft.setCursor(x, y);
  tft.print(s);
}

// Centred text; drops to size 1 when size 2 does not fit the screen width.
void textC(int y, const char* s, uint8_t size = 1, uint16_t color = C_TEXT) {
  if (size > 1 && (int)strlen(s) * 6 * size > SW - 8) size = 1;
  text((SW - (int)strlen(s) * 6 * size) / 2, y, s, size, color);
}

void header(const char* title) {
  tft.fillScreen(C_BG);
  tft.fillRect(0, 0, SW, 28, C_PANEL);
  tft.drawFastHLine(0, 28, SW, C_ACCENT);
  // title on the left, battery on the right (x >= SW - 64)
  const uint8_t size = (int)strlen(title) * 12 <= SW - 76 ? 2 : 1;
  text(8, size == 2 ? 6 : 10, title, size);
  drawBattery();
}

// Battery icon + percentage in the top right corner of the header.
void drawBattery() {
  if (screen == Screen::ABOUT) return;         // the About screen has no header bar (its title sits there)
  tft.fillRect(SW - 64, 3, 62, 22, C_PANEL);   // wide enough for "100%": no digit of an older value stays
  const int pct = power::batteryPercent();
  if (pct < 0) return;
  const int x = SW - 28, y = 8;
  tft.drawRect(x, y, 22, 12, C_DIM);
  tft.fillRect(x + 22, y + 3, 2, 6, C_DIM);
  tft.fillRect(x + 2, y + 2, max(1, 18 * pct / 100), 8, pct <= 15 ? C_RED : pct <= 35 ? C_WARN : C_ACCENT);
  char t[12];
  snprintf(t, sizeof t, "%d%%", pct);
  text(x - 4 - (int)strlen(t) * 6, y + 2, t, 1, C_DIM);
}

void drawBtn(const Btn& b, uint16_t fill = C_PANEL, uint16_t fg = C_TEXT) {
  tft.fillRoundRect(b.x, b.y, b.w, b.h, 6, fill);
  tft.drawRoundRect(b.x, b.y, b.w, b.h, 6, C_EDGE);
  const uint8_t size = (int)strlen(b.label) * 12 + 8 <= b.w && b.h >= 24 ? 2 : 1;
  const int tw = strlen(b.label) * 6 * size, th = 8 * size;
  text(b.x + (b.w - tw) / 2, b.y + (b.h - th) / 2, b.label, size, fg);
}

bool hit(const Btn& b, int x, int y) {
  return x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h;
}

void message(const char* title, const char* line, uint16_t color, uint32_t ms) {
  tft.fillRoundRect(8, 120, SW - 16, 80, 8, C_PANEL);
  tft.drawRoundRect(8, 120, SW - 16, 80, 8, color);
  textC(136, title, 2, color);
  textC(170, line, 1, C_TEXT);
  delay(ms);
}

// Spectrum |X[k]| on a linear scale (the biggest bin = full height), frequency axis in kHz, optional marker.
void drawSpectrum(int x0, int y0, int w, int h, const float* mag, int n, float fs, float markF) {
  tft.fillRect(x0, y0, w, h, C_PANEL);
  const int nb = n / 2;
  float mx = 1e-3f;
  for (int k = 1; k < nb; ++k) mx = max(mx, mag[k]);
  for (int px = 0; px < w; ++px) {
    const int k0 = 1 + px * (nb - 1) / w;
    const int k1 = max(k0 + 1, 1 + (px + 1) * (nb - 1) / w);
    float m = 0;
    for (int k = k0; k < k1 && k < nb; ++k) m = max(m, mag[k]);
    const int bar = constrain((int)(m / mx * (h - 12)), 0, h - 12);
    if (bar > 0) tft.drawFastVLine(x0 + px, y0 + h - 12 - bar, bar, C_ACCENT);
  }
  const float fMax = fs / 2;
  const float step = fMax <= 30000 ? 5000 : fMax <= 125000 ? 20000 : 50000;
  char lab[8];
  for (float f = 0; f <= fMax; f += step) {
    const int x = x0 + (int)(f / fMax * (w - 1));
    tft.drawFastVLine(x, y0 + h - 12, 3, C_DIM);
    snprintf(lab, sizeof lab, "%d", (int)(f / 1000));
    text(min(x + 2, x0 + w - 18), y0 + h - 9, lab, 1, C_DIM);
  }
  if (markF > 0 && markF <= fMax) {
    const int x = x0 + (int)(markF / fMax * (w - 1));
    tft.drawFastVLine(x, y0, h - 12, C_WARN);
  }
}

// ---------------- Touch helpers ----------------
bool getTap(int& x, int& y) {
  static bool down = false;
  int tx, ty;
  if (touch::read(tx, ty)) {
    if (down) return false;
    down = true;
    power::activity();
    x = tx;
    y = ty;
    return true;
  }
  down = false;
  return false;
}

void waitRelease() {
  int x, y, freeCount = 0;
  const uint32_t t0 = millis();
  while (freeCount < 3 && millis() - t0 < 3000) {   // give up after 3 s (stuck touch line)
    freeCount = touch::read(x, y) ? 0 : freeCount + 1;
    delay(10);
  }
}

bool anyTouch() {
  int x, y;
  return touch::read(x, y);
}

// ---------------- HOME ----------------
const Btn B_CAPTURE = {10, 36, 220, 54, "Capture"};
const Btn B_SAVED = {10, 96, 106, 46, "Remotes"};
const Btn B_SCENES = {124, 96, 106, 46, "Scenes"};
const Btn B_LAST = {10, 148, 106, 46, "Result"};
const Btn B_EXPER = {124, 148, 106, 46, "Sampling"};
const Btn B_MONITOR = {10, 200, 106, 46, "Monitor"};
const Btn B_THEME = {124, 200, 106, 46, "Theme"};
const Btn B_CAL = {10, 252, 106, 30, "Touch cal"};
const Btn B_ABOUT = {124, 252, 106, 30, "About"};

void showHome() {
  screen = Screen::HOME;
  header("IR Cloner");
  drawBtn(B_CAPTURE, C_ACCENT, C_INK);
  drawBtn(B_SAVED);
  drawBtn(B_SCENES);
  drawBtn(B_LAST, haveCur ? C_PANEL : C_OFF, haveCur ? C_TEXT : C_DIM);
  drawBtn(B_EXPER);
  drawBtn(B_MONITOR);
  drawBtn(B_THEME);
  drawBtn(B_CAL);
  drawBtn(B_ABOUT);
  char line[48];
  snprintf(line, sizeof line, "ADC %.0f kSa/s on GPIO35", adcStream::measuredFs() / 1000);
  textC(290, line, 1, C_DIM);
  textC(304, haveCur ? dec.line1 : "No signal yet", 1, C_DIM);
}

void onHome(int x, int y) {
  if (hit(B_CAPTURE, x, y)) doCapture(false);
  else if (hit(B_SAVED, x, y)) {
    pick = Pick::NONE;
    showRemotes();
  }
  else if (hit(B_SCENES, x, y)) showScenes();
  else if (hit(B_LAST, x, y) && haveCur) showResult();
  else if (hit(B_EXPER, x, y)) showExper();
  else if (hit(B_MONITOR, x, y)) runMonitor(false);
  else if (hit(B_THEME, x, y)) showTheme();
  else if (hit(B_ABOUT, x, y)) showAbout();
  else if (hit(B_CAL, x, y)) {
    runCalibration();
    showHome();
  }
}

// ---------------- CAPTURE ----------------
// Draws the capture prompt and waits up to 15 s for a remote key: timings and measured carrier go to dst.
irrx::Result captureInto(IrSignal& dst, bool fromSerial, const char* what, const char* cancelHint, float* snr) {
  header("Capture");
  textC(90, "Point the remote at", 2);
  textC(112, "the IR board and", 2);
  textC(134, what, 2);
  textC(170, "waiting 15 s ...", 1, C_WARN);
  textC(200, cancelHint, 1, C_DIM);
  if (fromSerial) Serial.println("Waiting for a remote (15 s)...");
  else waitRelease();                  // the finger that pressed "Capture" must not cancel it
  const irrx::Result r = irrx::capture(dst, 15000, fromSerial ? nullptr : anyTouch);
  if (r == irrx::Result::OK) dst.carrierHz = lroundf(irrx::estimateCarrier(snr));
  return r;
}

// Captures into cur (the signal of the result screen). what = last prompt line ("press one key").
bool captureSignal(bool fromSerial, const char* what) {
  float snr = 0;
  const irrx::Result r = captureInto(cur, fromSerial, what, "Touch the screen to cancel", &snr);
  if (r != irrx::Result::OK) {
    const char* why = r == irrx::Result::TIMEOUT ? "No signal in 15 s"
                    : r == irrx::Result::CANCELLED ? "Cancelled" : "ADC error";
    Serial.println(why);
    message("No capture", why, C_WARN, 1200);
    waitRelease();
    return false;
  }
  curSnr = snr;
  haveCur = true;
  rawValid = true;
  curName[0] = 0;
  irdecode::decode(cur, dec);
  printSignal(cur);
  return true;
}

// Number of durations that differ by more than 25 % (at least 150 us); -1 when the edge counts differ by more than 2.
int countDiff(const IrSignal& a, const IrSignal& b) {
  if (abs((int)a.n - (int)b.n) > 2) return -1;
  int d = abs((int)a.n - (int)b.n);
  for (int i = 0; i < min(a.n, b.n); ++i) {
    const int x = a.us[i], y = b.us[i];
    if (abs(x - y) > max(150, max(x, y) / 4)) ++d;
  }
  return d;
}

// Same key? Same NEC code, or for any other remote the same timings.
bool sameSignal(const IrSignal& a, const IrSignal& b) {
  IrDecoded da, db;
  irdecode::decode(a, da);
  irdecode::decode(b, db);
  if (da.proto != IrDecoded::UNKNOWN && da.proto == db.proto) {
    return da.bits == db.bits && memcmp(da.data, db.data, sizeof da.data) == 0;
  }
  const int d = countDiff(a, b);
  return d >= 0 && d <= 2;
}

void doCapture(bool fromSerial) {
  if (captureSignal(fromSerial, "press one key")) showResult();
  else showHome();
}

// ---------------- RESULT ----------------
const Btn B_SEND = {5, 216, 74, 46, "Send"};
const Btn B_SAVE = {83, 216, 74, 46, "Save"};
const Btn B_STEPS = {161, 216, 74, 46, "Steps"};
const Btn B_ANALYZE = {5, 268, 74, 46, "FFT"};
const Btn B_MATH = {83, 268, 74, 46, "Math"};
const Btn B_HOME = {161, 268, 74, 46, "Home"};

void drawTimeline(const IrSignal& s, int x0, int y0, int w, int h) {
  tft.fillRect(x0, y0, w, h, C_PANEL);
  uint32_t total = 0;
  for (int i = 0; i < s.n; ++i) total += s.us[i];
  if (!total) return;
  const float scale = float(w - 8) / total;
  float x = x0 + 4;
  for (int i = 0; i < s.n; ++i) {
    const float dw = s.us[i] * scale;
    if (i % 2 == 0) tft.fillRect((int)x, y0 + 6, max(1, (int)lroundf(dw)), h - 22, C_ACCENT);
    x += dw;
  }
  tft.drawFastHLine(x0 + 4, y0 + h - 16, w - 8, C_DIM);
  char lab[24];
  snprintf(lab, sizeof lab, "%.1f ms", total / 1000.0f);
  text(x0 + w - 6 - strlen(lab) * 6, y0 + h - 11, lab, 1, C_DIM);
  text(x0 + 4, y0 + h - 11, "0", 1, C_DIM);
}

void showResult() {
  screen = Screen::RESULT;
  header(curName[0] ? curName : "Captured");

  char line[56];
  if (cur.carrierHz) snprintf(line, sizeof line, "Carrier %.1f kHz", cur.carrierHz / 1000.0f);
  else snprintf(line, sizeof line, "Carrier ? (38k)");
  textC(38, line, 2, cur.carrierHz ? C_ACCENT : C_WARN);
  textC(64, dec.line1, 1);
  textC(78, dec.line2, 1, C_DIM);
  uint32_t total = 0;
  for (int i = 0; i < cur.n; ++i) total += cur.us[i];
  snprintf(line, sizeof line, "%u edges, %.1f ms", cur.n, total / 1000.0f);
  textC(92, line, 1, C_DIM);

  drawTimeline(cur, 5, 120, 230, 88);
  drawBtn(B_SEND, C_ACCENT, C_INK);
  drawBtn(B_SAVE);
  drawBtn(B_ANALYZE, rawValid ? C_PANEL : C_OFF, rawValid ? C_TEXT : C_DIM);
  drawBtn(B_STEPS, rawValid ? C_PANEL : C_OFF, rawValid ? C_TEXT : C_DIM);
  drawBtn(B_MATH, rawValid ? C_PANEL : C_OFF, rawValid ? C_TEXT : C_DIM);
  drawBtn(B_HOME);
}

void onResult(int x, int y) {
  if (hit(B_SEND, x, y)) {
    drawBtn(B_SEND, C_WARN, C_INK);
    doSend(cur);
    drawBtn(B_SEND, C_ACCENT, C_INK);
  } else if (hit(B_SAVE, x, y)) {
    pick = Pick::SAVE;        // choose the remote, then name the key
    showRemotes();
  } else if (hit(B_STEPS, x, y) && rawValid) {
    showSteps();
  } else if (hit(B_MATH, x, y) && rawValid) {
    mathPage = 0;
    showMath();
  } else if (hit(B_ANALYZE, x, y) && rawValid) {
    showAnalyze();
  } else if (hit(B_HOME, x, y)) {
    showHome();
  }
}

// Sends the cleaned version of a captured signal (exact NEC timings / merged durations, standard carrier).
void doSend(const IrSignal& s, bool log) {
  static IrSignal tx;
  irdecode::prepareForSend(s, tx);
  const bool ok = irtx::send(tx);
  if (!log) return;                     // repeats: printing ~600 numbers would slow them down
  Serial.printf("Send %s (%u edges, carrier %lu Hz)\nTXDUR", ok ? "OK" : "FAILED", tx.n, (unsigned long)tx.carrierHz);
  for (int i = 0; i < tx.n; ++i) Serial.printf(",%u", tx.us[i]);
  Serial.println();
}

// ---------------- ANALYZE (waveform + FFT of the captured signal) ----------------
const Btn B_BACK_A = {5, 280, 230, 36, "Back"};

// Spectrum of the raw samples inside the first mark (from 0.2 ms after it starts). Returns N, 0 if too short.
int markSpectrum(float*& mag, int& s0) {
  const float fs = irrx::rawFs();
  s0 = irrx::rawTrigger() + (int)(fs * 200e-6f);
  int n = FFT_MAX_N;
  while (n >= 128 && s0 + n > irrx::rawCount()) n /= 2;
  if (n < 128) return 0;
  mag = dsp::spectrum(irrx::raw() + s0, n);
  return n;
}

void showAnalyze() {
  screen = Screen::ANALYZE;
  header("Waveform + spectrum");
  const uint16_t* raw = irrx::raw();
  const float fs = irrx::rawFs();

  // Waveform: 220 raw samples from just before the first mark (1.1 ms at 200 kSa/s).
  const int x0 = 5, y0 = 34, w = 230, h = 96;
  tft.fillRect(x0, y0, w, h, C_PANEL);
  const int start = max(0, irrx::rawTrigger() - 20);
  const int count = min(w - 10, irrx::rawCount() - start);
  int lo = 4095, hi = 0;
  for (int i = 0; i < count; ++i) {
    lo = min(lo, (int)raw[start + i]);
    hi = max(hi, (int)raw[start + i]);
  }
  const int span = max(1, hi - lo);
  int py = 0;
  for (int i = 0; i < count; ++i) {
    const int yy = y0 + h - 4 - (raw[start + i] - lo) * (h - 18) / span;
    if (i) tft.drawLine(x0 + 4 + i, py, x0 + 5 + i, yy, C_ACCENT);
    py = yy;
  }
  char line[64];
  snprintf(line, sizeof line, "raw %.2f ms  (%d..%d)", count * 1000.0f / fs, lo, hi);
  text(x0 + 4, y0 + 3, line, 1, C_DIM);

  // Spectrum of the samples inside the first mark.
  float* mag = nullptr;
  int s0 = 0;
  const int n = markSpectrum(mag, s0);
  float fpk = 0;
  if (n) {
    fpk = dsp::peak(mag, n, fs, 25000.0f, min(60000.0f, fs / 2), nullptr);
    drawSpectrum(5, 136, 230, 100, mag, n, fs, fpk);
  }
  snprintf(line, sizeof line, "Carrier peak (a1) %.2f kHz", fpk / 1000);
  textC(240, line, 1);
  snprintf(line, sizeof line, "w0 Ts = 2 pi f0 / fs = %.2f pi", 2 * fpk / fs);
  textC(252, line, 1, C_DIM);
  snprintf(line, sizeof line, "fs %.1f kSa/s  N=%d  df=fs/N=%.0f Hz", fs / 1000, n, n ? fs / n : 0.0f);
  textC(264, line, 1, C_DIM);
  drawBtn(B_BACK_A);
}

void onAnalyze(int x, int y) {
  if (hit(B_BACK_A, x, y)) showResult();
}

// ---------------- Dialogs: on-screen keyboard (names) and yes / no ----------------
const char* const KB_ROWS[4] = {"1234567890", "QWERTYUIOP", "ASDFGHJKL-", "ZXCVBNM_./"};
const Btn B_KB_CASE = {2, 230, 56, 34, "abc"};
const Btn B_KB_SPACE = {62, 230, 116, 34, "Space"};
const Btn B_KB_DEL = {182, 230, 56, 34, "Del"};
const Btn B_KB_CANCEL = {2, 272, 114, 44, "Cancel"};
const Btn B_KB_OK = {124, 272, 114, 44, "OK"};

Btn kbKey(int row, int col) { return {(int16_t)(2 + col * 24), (int16_t)(78 + row * 38), 22, 34, ""}; }

void drawNameField(const char* s) {
  tft.fillRoundRect(4, 34, 232, 38, 6, C_PANEL);
  tft.drawRoundRect(4, 34, 232, 38, 6, C_ACCENT);
  text(12, 46, s, 2);
  tft.fillRect(12 + strlen(s) * 12, 44, 2, 18, C_WARN);   // cursor
}

void drawKeyboard(bool lower) {
  char lab[2] = {0, 0};
  for (int r = 0; r < 4; ++r) {
    for (int c = 0; KB_ROWS[r][c]; ++c) {
      Btn k = kbKey(r, c);
      lab[0] = lower ? (char)tolower(KB_ROWS[r][c]) : KB_ROWS[r][c];
      k.label = lab;
      drawBtn(k);
    }
  }
  Btn cs = B_KB_CASE;
  cs.label = lower ? "ABC" : "abc";
  drawBtn(cs);
  drawBtn(B_KB_SPACE);
  drawBtn(B_KB_DEL);
  drawBtn(B_KB_CANCEL);
  drawBtn(B_KB_OK, C_ACCENT, C_INK);
}

// Lets the user type a name. buf (NAME_LEN bytes) holds the start text and the result. false = Cancel.
bool editName(const char* title, char* buf) {
  header(title);
  bool lower = false;
  drawNameField(buf);
  drawKeyboard(lower);
  waitRelease();
  while (true) {
    int x, y;
    if (!getTap(x, y)) {
      delay(15);
      continue;
    }
    int len = strlen(buf);
    if (hit(B_KB_OK, x, y)) {
      while (len && buf[len - 1] == ' ') buf[--len] = 0;
      if (len) return true;                         // an empty name is not accepted
    } else if (hit(B_KB_CANCEL, x, y)) {
      return false;
    } else if (hit(B_KB_CASE, x, y)) {
      lower = !lower;
      drawKeyboard(lower);
    } else if (hit(B_KB_DEL, x, y)) {
      if (len) buf[len - 1] = 0;
    } else if (hit(B_KB_SPACE, x, y)) {
      if (len && len < NAME_LEN - 1) {
        buf[len] = ' ';
        buf[len + 1] = 0;
      }
    } else if (len < NAME_LEN - 1) {
      for (int r = 0; r < 4; ++r)
        for (int c = 0; KB_ROWS[r][c]; ++c)
          if (hit(kbKey(r, c), x, y)) {
            buf[len] = lower ? (char)tolower(KB_ROWS[r][c]) : KB_ROWS[r][c];
            buf[len + 1] = 0;
          }
    }
    drawNameField(buf);
  }
}

bool confirmBox(const char* question) {
  const Btn yes = {20, 168, 95, 40, "Yes"}, no = {125, 168, 95, 40, "No"};
  tft.fillRoundRect(8, 110, SW - 16, 112, 8, C_PANEL);
  tft.drawRoundRect(8, 110, SW - 16, 112, 8, C_RED);
  textC(130, question, 2, C_RED);
  drawBtn(yes, C_RED, C_INK);
  drawBtn(no);
  waitRelease();
  while (true) {
    int x, y;
    if (!getTap(x, y)) {
      delay(15);
      continue;
    }
    if (hit(yes, x, y)) return true;
    if (hit(no, x, y)) return false;
  }
}

// ---------------- REMOTES (list of saved remotes) ----------------
const Btn B_NEW_R = {5, 276, 112, 40, "+ New"};
const Btn B_BACK_R = {123, 276, 112, 40, "Back"};
int rowRemote[MAX_REMOTES];      // remote shown in each row
int rowCount = 0;

Btn remoteRow(int i) { return {5, (int16_t)(34 + i * 40), 230, 36, ""}; }

void showRemotes() {
  screen = Screen::REMOTES;
  header(pick == Pick::SAVE ? "Save to remote" : pick == Pick::STEP ? "Pick a remote" : "Remotes");
  rowCount = 0;
  char name[NAME_LEN], line[16];
  for (int r = 0; r < MAX_REMOTES; ++r) {
    if (!store::remoteName(r, name)) continue;
    const Btn row = remoteRow(rowCount);
    rowRemote[rowCount++] = r;
    tft.fillRoundRect(row.x, row.y, row.w, row.h, 6, C_PANEL);
    tft.drawRoundRect(row.x, row.y, row.w, row.h, 6, C_EDGE);
    text(row.x + 10, row.y + 11, name, 2);
    snprintf(line, sizeof line, "%d keys", store::buttonCount(r));
    text(row.x + row.w - 8 - strlen(line) * 6, row.y + 15, line, 1, C_DIM);
  }
  if (!rowCount) {
    textC(110, "No remotes yet", 2, C_DIM);
    textC(140, "Press + New and give it a name", 1, C_DIM);
  }
  const bool full = rowCount >= MAX_REMOTES || pick == Pick::STEP;   // a scene step needs an existing key
  drawBtn(B_NEW_R, full ? C_OFF : C_ACCENT, full ? C_DIM : C_INK);
  drawBtn(B_BACK_R);
}

void openRemote(int r) {
  curRemote = r;
  editMode = false;
  showRemote();
}

// Stores cur (the signal on the result screen) as a new key of remote r. The key name is asked first.
void saveCurTo(int r) {
  pick = Pick::NONE;
  const int b = store::freeButton(r);
  if (b < 0) {
    message("Remote full", "Delete a key first", C_WARN, 1500);
    showResult();
    return;
  }
  char name[NAME_LEN] = "";
  if (!editName("Key name", name)) {
    showResult();
    return;
  }
  if (!store::saveButton(r, b, name, cur)) {
    message("Error", "Could not write flash", C_RED, 1500);
    showResult();
    return;
  }
  strcpy(curName, name);
  message("Saved", name, C_ACCENT, 800);
  openRemote(r);
}

void onRemotes(int x, int y) {
  if (hit(B_BACK_R, x, y)) {
    const Pick p = pick;
    pick = Pick::NONE;
    if (p == Pick::SAVE) showResult();
    else if (p == Pick::STEP) showScene();
    else showHome();
    return;
  }
  if (hit(B_NEW_R, x, y)) {
    const int r = store::freeRemote();
    if (r < 0 || pick == Pick::STEP) return;
    char name[NAME_LEN] = "";
    if (!editName("Remote name", name) || !store::saveRemoteName(r, name)) showRemotes();
    else if (pick == Pick::SAVE) saveCurTo(r);
    else openRemote(r);
    return;
  }
  for (int i = 0; i < rowCount; ++i) {
    if (!hit(remoteRow(i), x, y)) continue;
    if (pick == Pick::SAVE) saveCurTo(rowRemote[i]);
    else openRemote(rowRemote[i]);
    return;
  }
}

// ---------------- REMOTE (the keys of one remote: tap = send) ----------------
const Btn B_ADD = {5, 268, 74, 46, "+ Add"};
const Btn B_EDIT = {83, 268, 74, 46, "Edit"};
const Btn B_BACK_K = {161, 268, 74, 46, "Back"};
const Btn B_REN_R = {5, 268, 74, 46, "Rename"};
const Btn B_DEL_R = {83, 268, 74, 46, "Delete"};
const Btn B_DONE = {161, 268, 74, 46, "Done"};
char keyName[MAX_BUTTONS][NAME_LEN];
bool keyUsed[MAX_BUTTONS];

Btn keyBtn(int b) { return {(int16_t)(5 + (b % 2) * 118), (int16_t)(34 + (b / 2) * 38), 112, 34, keyName[b]}; }

void drawKey(int b, bool busy = false) {
  if (busy) drawBtn(keyBtn(b), C_WARN, C_INK);
  else drawBtn(keyBtn(b), C_PANEL, editMode ? C_WARN : C_TEXT);
}

void showRemote() {
  screen = Screen::REMOTE;
  char name[NAME_LEN] = "?", title[24];
  store::remoteName(curRemote, name);
  if (pick == Pick::STEP) snprintf(title, sizeof title, "Pick a key");
  else snprintf(title, sizeof title, editMode ? "Edit %s" : "%s", name);
  header(title);
  int used = 0;
  for (int b = 0; b < MAX_BUTTONS; ++b) {
    keyUsed[b] = store::buttonName(curRemote, b, keyName[b]);
    if (!keyUsed[b]) continue;
    ++used;
    drawKey(b);
  }
  if (!used) {
    textC(110, "No keys yet", 2, C_DIM);
    textC(140, "Press + Add, then press the key", 1, C_DIM);
    textC(154, "on the original remote", 1, C_DIM);
  }
  if (pick == Pick::STEP) {
    drawBtn(B_BACK_K);
  } else if (editMode) {
    if (used) textC(259, "tap a key to change it", 1, C_WARN);
    drawBtn(B_REN_R);
    drawBtn(B_DEL_R, C_PANEL, C_RED);
    drawBtn(B_DONE, C_ACCENT, C_INK);
  } else {
    const bool full = used >= MAX_BUTTONS;
    drawBtn(B_ADD, full ? C_OFF : C_ACCENT, full ? C_DIM : C_INK);
    drawBtn(B_EDIT);
    drawBtn(B_BACK_K);
  }
}

// Copies key b of the current remote from the original remote: name (new keys only), capture, test, save.
const Btn B_L_TEST = {5, 200, 112, 50, "Test"};
const Btn B_L_SAVE = {123, 200, 112, 50, "Save"};
const Btn B_L_RETRY = {5, 258, 112, 50, "Retry"};
const Btn B_L_CANCEL = {123, 258, 112, 50, "Cancel"};

void learnKey(int b) {
  char name[NAME_LEN] = "";
  if (keyUsed[b]) {
    strcpy(name, keyName[b]);
  } else if (!editName("Key name", name)) {
    showRemote();
    return;
  }
  char what[24];
  snprintf(what, sizeof what, "press %s", name);
  while (true) {
    if (!captureSignal(false, what)) {
      showRemote();
      return;
    }
    // Second press of the same key: both captures must agree, so a bad copy shows up before it is saved.
    float snr2 = 0;
    int check = 0;                                 // 0 skipped, 1 same, -1 different
    if (captureInto(keySig, false, "press it again", "Touch the screen to skip the check", &snr2) == irrx::Result::OK) {
      check = sameSignal(cur, keySig) ? 1 : -1;
    } else {
      rawValid = false;                            // the raw buffer may hold part of the second attempt
      waitRelease();
    }
    header(name);
    textC(40, dec.line1, 1);
    textC(54, dec.line2, 1, C_DIM);
    uint32_t total = 0;
    for (int i = 0; i < cur.n; ++i) total += cur.us[i];
    char line[40];
    snprintf(line, sizeof line, "%u edges, %.1f ms", cur.n, total / 1000.0f);
    textC(68, line, 1, C_DIM);
    drawTimeline(cur, 5, 82, 230, 88);
    if (check > 0) textC(174, "Check: both presses match", 1, C_ACCENT);
    else if (check < 0) textC(174, "Check: the presses differ - Retry", 1, C_WARN);
    else textC(174, "Check skipped", 1, C_DIM);
    textC(186, "AC: copy ON and OFF as two keys", 1, C_DIM);
    drawBtn(B_L_TEST);
    drawBtn(B_L_SAVE, C_ACCENT, C_INK);
    drawBtn(B_L_RETRY);
    drawBtn(B_L_CANCEL);
    waitRelease();
    bool retry = false;
    while (!retry) {
      int x, y;
      if (!getTap(x, y)) {
        delay(15);
        continue;
      }
      if (hit(B_L_TEST, x, y)) {
        drawBtn(B_L_TEST, C_WARN, C_INK);
        doSend(cur);
        drawBtn(B_L_TEST);
      } else if (hit(B_L_SAVE, x, y)) {
        if (store::saveButton(curRemote, b, name, cur)) message("Saved", name, C_ACCENT, 700);
        else message("Error", "Could not write flash", C_RED, 1500);
        showRemote();
        return;
      } else if (hit(B_L_RETRY, x, y)) {
        retry = true;
      } else if (hit(B_L_CANCEL, x, y)) {
        showRemote();
        return;
      }
    }
  }
}

void onRemote(int x, int y) {
  for (int b = 0; b < MAX_BUTTONS; ++b) {
    if (!keyUsed[b] || !hit(keyBtn(b), x, y)) continue;
    if (pick == Pick::STEP) {
      addSceneStep(curRemote, b);
    } else if (editMode) {
      menuKey = b;
      showKeyMenu();
    } else if (store::loadButton(curRemote, b, keySig)) {
      drawKey(b, true);
      doSend(keySig);
      // Key held down: sent again and again like a real remote (volume, channel). 3 free readings = released.
      const uint32_t t0 = millis();
      for (int up = 0; up < 3;) {
        if (!anyTouch()) {
          ++up;
          delay(10);
          continue;
        }
        up = 0;
        if (millis() - t0 < HOLD_REPEAT_MS) {
          delay(10);
          continue;
        }
        doSend(keySig, false);
        delay(REPEAT_GAP_MS);
      }
      drawKey(b);
    }
    return;
  }
  if (pick == Pick::STEP) {
    if (hit(B_BACK_K, x, y)) showRemotes();
    return;
  }
  if (editMode) {
    if (hit(B_DONE, x, y)) {
      editMode = false;
      showRemote();
    } else if (hit(B_REN_R, x, y)) {
      char name[NAME_LEN] = "";
      store::remoteName(curRemote, name);
      if (editName("Remote name", name)) store::saveRemoteName(curRemote, name);
      showRemote();
    } else if (hit(B_DEL_R, x, y)) {
      if (confirmBox("Delete remote?")) {
        store::removeRemote(curRemote);
        curRemote = -1;
        editMode = false;
        showRemotes();
      } else {
        showRemote();
      }
    }
  } else if (hit(B_ADD, x, y)) {
    const int b = store::freeButton(curRemote);
    if (b >= 0) learnKey(b);
  } else if (hit(B_EDIT, x, y)) {
    editMode = true;
    showRemote();
  } else if (hit(B_BACK_K, x, y)) {
    showRemotes();
  }
}

// ---------------- KEY (change one key of a remote) ----------------
const Btn B_K_SEND = {10, 122, 220, 40, "Test send"};
const Btn B_K_COPY = {10, 168, 220, 40, "Copy again"};
const Btn B_K_RENAME = {10, 214, 106, 40, "Rename"};
const Btn B_K_DELETE = {124, 214, 106, 40, "Delete"};
const Btn B_K_DETAILS = {10, 270, 106, 44, "Details"};
const Btn B_K_BACK = {124, 270, 106, 44, "Back"};

void showKeyMenu() {
  screen = Screen::KEY;
  header(keyName[menuKey]);
  if (store::loadButton(curRemote, menuKey, keySig)) {
    IrDecoded d;
    irdecode::decode(keySig, d);
    textC(40, d.line1, 1);
    textC(54, d.line2, 1, C_DIM);
    drawTimeline(keySig, 5, 70, 230, 46);
  }
  drawBtn(B_K_SEND, C_ACCENT, C_INK);
  drawBtn(B_K_COPY);
  drawBtn(B_K_RENAME);
  drawBtn(B_K_DELETE, C_PANEL, C_RED);
  drawBtn(B_K_DETAILS);
  drawBtn(B_K_BACK);
}

void onKeyMenu(int x, int y) {
  if (hit(B_K_SEND, x, y)) {
    if (store::loadButton(curRemote, menuKey, keySig)) {
      drawBtn(B_K_SEND, C_WARN, C_INK);
      doSend(keySig);
      drawBtn(B_K_SEND, C_ACCENT, C_INK);
    }
  } else if (hit(B_K_COPY, x, y)) {
    learnKey(menuKey);
  } else if (hit(B_K_RENAME, x, y)) {
    char name[NAME_LEN];
    strcpy(name, keyName[menuKey]);
    if (editName("Key name", name) && store::renameButton(curRemote, menuKey, name)) strcpy(keyName[menuKey], name);
    showKeyMenu();
  } else if (hit(B_K_DELETE, x, y)) {
    if (confirmBox("Delete key?")) {
      store::removeButton(curRemote, menuKey);
      showRemote();
    } else {
      showKeyMenu();
    }
  } else if (hit(B_K_DETAILS, x, y)) {
    if (store::loadButton(curRemote, menuKey, cur)) {
      haveCur = true;
      rawValid = false;
      strcpy(curName, keyName[menuKey]);
      irdecode::decode(cur, dec);
      printSignal(cur);
      showResult();
    }
  } else if (hit(B_K_BACK, x, y)) {
    showRemote();
  }
}

// ---------------- STEPS (how the receiver turns light into bits, for the last capture) ----------------
const Btn B_BACK_P = {5, 286, 230, 30, "Back"};

void showSteps() {
  screen = Screen::STEPS;
  header("Signal steps");
  const uint16_t* raw = irrx::raw();
  const int cnt = irrx::rawCount(), trig = irrx::rawTrigger();
  const float fs = irrx::rawFs();
  char line[64];

  // 1: raw ADC samples from just before the first mark: the carrier cycles are visible
  text(5, 32, "1 Photodiode -> ADC (first 1 ms)", 1, C_DIM);
  tft.fillRect(5, 42, 230, 46, C_PANEL);
  const int start = max(0, trig - 20), count = min((int)(fs * 1e-3f), cnt - start);
  int lo = 4095, hi = 0;
  for (int i = 0; i < count; ++i) {
    lo = min(lo, (int)raw[start + i]);
    hi = max(hi, (int)raw[start + i]);
  }
  int px = 0, py = 0;
  for (int i = 0; i < count; ++i) {
    const int xx = 7 + i * 226 / max(1, count - 1);
    const int yy = 85 - (raw[start + i] - lo) * 40 / max(1, hi - lo);
    if (i) tft.drawLine(px, py, xx, yy, C_ACCENT);
    px = xx;
    py = yy;
  }

  // 2: envelope = strongest sample of every 50 us window; the demodulator thresholds this
  const int W = max(2, (int)lroundf(fs * WINDOW_US * 1e-6f)), nw = cnt / W;
  auto env = [&](int w) {
    int m = 0;
    for (int k = 0; k < W; ++k) m = max(m, (int)raw[w * W + k]);
    return m;
  };
  int elo = 4095, ehi = 0;
  for (int w = 0; w < nw; ++w) {
    const int e = env(w);
    elo = min(elo, e);
    ehi = max(ehi, e);
  }
  snprintf(line, sizeof line, "2 Envelope (max / %u us), %.0f ms", (unsigned)WINDOW_US, cnt * 1000.0f / fs);
  text(5, 92, line, 1, C_DIM);
  tft.fillRect(5, 102, 230, 46, C_PANEL);
  for (int x = 0; x < 226 && nw; ++x) {
    const int w0 = x * nw / 226, w1 = max(w0 + 1, (x + 1) * nw / 226);
    int e = 0;
    for (int w = w0; w < w1 && w < nw; ++w) e = max(e, env(w));
    const int bar = (e - elo) * 40 / max(1, ehi - elo);
    if (bar > 0) tft.drawFastVLine(7 + x, 145 - bar, bar, C_ACCENT);
  }
  for (int x = 7; x < 233; x += 6) tft.drawFastHLine(x, 125, 3, C_WARN);   // 50 % threshold: mark or space

  // 3: mark / space durations of the whole frame
  text(5, 152, "3 Threshold -> marks and spaces", 1, C_DIM);
  drawTimeline(cur, 5, 162, 230, 44);
  int len = snprintf(line, sizeof line, "us:");
  for (int i = 0; i < cur.n && len < 30; ++i) len += snprintf(line + len, sizeof line - len, " %u", cur.us[i]);
  if (cur.n) text(5, 210, line, 1, C_DIM);

  // 4: bits and the decoded code
  text(5, 224, "4 Durations -> bits -> code", 1, C_DIM);
  text(5, 236, dec.line1, 1);
  if (dec.bits) {
    for (int row = 0; row < 2; ++row) {
      char b[40];
      int k = 0;
      for (int i = row * 32; i < min<int>(dec.bits, row * 32 + 32); ++i) {
        if (i % 8 == 0 && i != row * 32) b[k++] = ' ';
        b[k++] = (dec.data[i / 8] >> (i % 8)) & 1 ? '1' : '0';
      }
      b[k] = 0;
      if (k) text(5, 248 + row * 12, b, 1);
    }
    text(5, 272, dec.bits > 64 ? "first 64 bits, in the order sent" : "bits in the order they were sent", 1, C_DIM);
  } else {
    text(5, 248, dec.line2, 1, C_DIM);
  }
  drawBtn(B_BACK_P);
}

void onSteps(int x, int y) {
  if (hit(B_BACK_P, x, y)) showResult();
}

// ---------------- MATH (convolution, frequency response and Z, computed from the last capture) ----------------
// Page 0: the receiver (photodiode + R1 + stray capacitance) as an LTI system with h(t) = (1/RC) e^(-t/RC) u(t).
//         RC is measured from the fall after the first mark; |H(jw)| = 1 / sqrt(1 + (w RC)^2) on linear axes;
//         convolution x(t) * h(t) against the measured samples.
// Page 1: a first-order recursive difference equation y[n] = a y[n-1] + (1 - a) x[n] run on the raw samples:
//         H(z), pole, zero, ROC, and |H(e^jw)| at the carrier.
const Btn B_TH_PAGE = {5, 276, 112, 40, ""};
const Btn B_TH_BACK = {123, 276, 112, 40, "Back"};
constexpr float DIGITAL_TAU_S = 50e-6f;   // time constant of the demo digital filter (one envelope window)

struct RcFit {
  bool ok = false;
  float tau = 0;           // seconds
  int i90 = 0, i10 = 0;    // raw sample indices of the 90 % and 10 % points of the fall
  float base = 0, top = 0;
};

// RC of the receiver (kept in f.tau) from the fall after the first mark: e^(-t/RC) falls from 90 % to 10 % in RC ln 9.
// The level is the max over one carrier period, so the 38 kHz ripple does not count as a fall.
RcFit fitRc() {
  RcFit f;
  if (!rawValid || cur.n < 1) return f;
  const uint16_t* raw = irrx::raw();
  const int cnt = irrx::rawCount(), trig = irrx::rawTrigger();
  const float fs = irrx::rawFs();
  const int P = max(2, (int)ceilf(fs / 36000.0f));
  auto level = [&](int i) {
    int m = 0;
    for (int k = 0; k < P && i + k < cnt; ++k) m = max(m, (int)raw[i + k]);
    return m;
  };
  float base = 0;                                  // dark level: the samples before the first mark
  for (int i = 0; i < trig; ++i) base += raw[i];
  base = trig ? base / trig : 0;
  const int markLen = (int)(cur.us[0] * fs * 1e-6f), mid = trig + markLen / 2;
  if (markLen < 4 * P || mid + 8 * P >= cnt) return f;
  float top = 0;
  int n = 0;
  for (int i = max(trig, mid - 4 * P); i < mid + 4 * P; i += P, ++n) top += level(i);
  top /= max(1, n);
  if (top - base < 60) return f;
  const float span = top - base;
  int i90 = -1, i10 = -1;
  for (int i = mid; i < cnt - P; ++i) {
    const int e = level(i);
    if (i90 < 0 && e < base + 0.9f * span) i90 = i;
    if (i90 >= 0 && e < base + 0.1f * span) {
      i10 = i;
      break;
    }
  }
  if (i90 < 0 || i10 <= i90) return f;
  f.ok = true;
  f.tau = (i10 - i90) / fs / logf(9.0f);
  f.i90 = i90;
  f.i10 = i10;
  f.base = base;
  f.top = top;
  return f;
}

// |H(jw)| = 1 / sqrt(1 + (w RC)^2) on linear axes: f = w / 2 pi from 0 to 50 kHz, |H| from 0 to 1.
void drawHmag(int x0, int y0, int w, int h, float rc) {
  tft.fillRect(x0, y0, w, h, C_PANEL);
  auto fx = [&](float f) { return x0 + 4 + (int)(f / 50000 * (w - 8)); };
  auto hy = [&](float v) { return y0 + h - 12 - (int)(v * (h - 18)); };
  for (float f : {10000.0f, 20000.0f, 30000.0f, 40000.0f}) tft.drawFastVLine(fx(f), y0 + 2, h - 14, C_EDGE);
  tft.drawFastHLine(x0 + 4, hy(0.707f), w - 8, C_EDGE);
  for (int y = y0 + 2; y < y0 + h - 12; y += 4) tft.drawPixel(fx(38000), y, C_TEXT);
  int px = 0, py = 0;
  for (int i = 0; i <= w - 8; ++i) {
    const float f = 50000.0f * i / (w - 8), wrc = 2 * PI * f * rc;
    const int x = x0 + 4 + i, y = hy(1 / sqrtf(1 + wrc * wrc));
    if (i) tft.drawLine(px, py, x, y, C_ACCENT);
    px = x;
    py = y;
  }
  text(x0 + 4, y0 + h - 10, "0", 1, C_DIM);
  text(fx(20000) - 5, y0 + h - 10, "20", 1, C_DIM);
  text(fx(40000) - 5, y0 + h - 10, "40", 1, C_DIM);
  text(x0 + w - 40, y0 + h - 10, "50 kHz", 1, C_DIM);
  text(x0 + 30, hy(0.707f) - 10, "0.707", 1, C_DIM);
  text(fx(38000) - 27, y0 + 4, "38k", 1, C_TEXT);
  text(x0 + w - 50, y0 + 14, "|H(jw)|", 1, C_ACCENT);
}

// Measured samples around the end of the first mark, and the model: a rectangular mark convolved with
// h(t) = (1 / RC) e^(-t / RC) u(t), which after the mark is a decay with the same RC.
void drawConvolution(int x0, int y0, int w, int h, const RcFit& f) {
  tft.fillRect(x0, y0, w, h, C_PANEL);
  const uint16_t* raw = irrx::raw();
  const int cnt = irrx::rawCount();
  const float fs = irrx::rawFs(), tauS = f.tau * fs;
  const int s0 = max(0, f.i90 - (int)(4 * tauS)), s1 = min(cnt - 1, f.i90 + max(10, (int)(8 * tauS)));
  const float te = f.i90 - tauS * logf(1 / 0.9f);   // end of the ideal mark, in samples
  auto Y = [&](float v) {
    return constrain(y0 + h - 5 - (int)((v - f.base) / max(1.0f, f.top - f.base) * (h - 16)), y0 + 2, y0 + h - 2);
  };
  int px = 0, py = 0, my = 0;
  for (int i = s0; i <= s1; ++i) {
    const int x = x0 + 4 + (i - s0) * (w - 8) / max(1, s1 - s0);
    const float model = f.base + (f.top - f.base) * (i < te ? 1.0f : expf(-(i - te) / tauS));
    const int y = Y(raw[i]), ym = Y(model);
    if (i > s0) {
      tft.drawLine(px, py, x, y, C_ACCENT);
      tft.drawLine(px, my, x, ym, C_WARN);
    }
    px = x;
    py = y;
    my = ym;
  }
  text(x0 + w - 58, y0 + 4, "measured", 1, C_ACCENT);
  text(x0 + w - 58, y0 + 14, "x * h", 1, C_WARN);
}

void showMath() {
  screen = Screen::MATH;
  char line[48];
  Btn page = B_TH_PAGE;
  const float fs = irrx::rawFs();
  if (mathPage == 0) {
    header("Sensor RC");
    const RcFit f = fitRc();
    if (!f.ok) {
      textC(90, "No clean fall", 2, C_WARN);
      textC(120, "after the first mark in this", 1, C_DIM);
      textC(132, "capture. Capture again closer:", 1, C_DIM);
      textC(144, "a long first mark works best", 1, C_DIM);
    } else {
      const float fc = 1 / (2 * PI * f.tau), wt = 2 * PI * 38000 * f.tau;
      snprintf(line, sizeof line, "RC = %.0f us", f.tau * 1e6f);
      textC(34, line, 2, C_ACCENT);
      textC(54, "h(t) = (1/RC) e^(-t/RC) u(t)", 1);
      snprintf(line, sizeof line, "H(jw) = 1/(1 + jwRC)   |H(38k)| = %.3f", 1 / sqrtf(1 + wt * wt));
      textC(66, line, 1, C_DIM);
      snprintf(line, sizeof line, "w = 1/RC (%.2f kHz): |H| = 0.707", fc / 1000);
      textC(78, line, 1, C_DIM);
      drawHmag(5, 90, 230, 80, f.tau);
      textC(174, "x(t) * h(t): end of the 1st mark", 1, C_DIM);
      drawConvolution(5, 184, 230, 70, f);
      snprintf(line, sizeof line, "fall 90%%->10%%: %.0f us = RC ln 9", (f.i10 - f.i90) / fs * 1e6f);
      textC(260, line, 1, C_DIM);
    }
    page.label = "Z  >";
  } else {
    header("Z-transform");
    const float a = expf(-1 / (fs * DIGITAL_TAU_S));
    const float w38 = 2 * PI * 38000 / fs;
    const float h38 = (1 - a) / sqrtf(1 - 2 * a * cosf(w38) + a * a);
    textC(34, "y[n] = a y[n-1] + (1-a) x[n]", 1);
    textC(46, "H(z) = (1-a) / (1 - a z^-1)", 1);
    snprintf(line, sizeof line, "a = %.3f  (%.0f us at %.0f kSa/s)", a, DIGITAL_TAU_S * 1e6f, fs / 1000);
    textC(58, line, 1, C_DIM);
    // z-plane: unit circle, zero at z = 0, pole at z = a
    const int cx = 53, cy = 120, r = 40;
    tft.fillRect(5, 72, 96, 96, C_PANEL);
    tft.drawFastHLine(9, cy, 88, C_EDGE);
    tft.drawFastVLine(cx, 76, 88, C_EDGE);
    tft.drawCircle(cx, cy, r, C_DIM);
    const int xp = cx + (int)lroundf(a * r);
    tft.drawLine(xp - 3, cy - 3, xp + 3, cy + 3, C_WARN);
    tft.drawLine(xp - 3, cy + 3, xp + 3, cy - 3, C_WARN);
    tft.drawCircle(cx, cy, 3, C_ACCENT);
    text(9, 76, "z-plane", 1, C_DIM);
    snprintf(line, sizeof line, "pole z = %.3f", a);
    text(108, 80, line, 1, C_WARN);
    text(108, 94, "zero z = 0", 1, C_ACCENT);
    text(108, 108, "ROC |z| > a", 1);
    text(108, 120, "has |z| = 1: stable", 1);
    snprintf(line, sizeof line, "|H(e^jw)| at %.2f pi", w38 / PI);
    text(108, 140, line, 1, C_DIM);
    snprintf(line, sizeof line, "(38 kHz) = %.3f", h38);
    text(108, 152, line, 1);
    // time domain: the raw samples of the first 1 ms (x[n]) and the filter output (y[n])
    textC(174, "x[n] raw (grey) -> y[n] filtered", 1, C_DIM);
    tft.fillRect(5, 184, 230, 70, C_PANEL);
    const uint16_t* raw = irrx::raw();
    const int cnt = irrx::rawCount(), start = max(0, irrx::rawTrigger() - 20);
    const int count = min((int)(fs * 1e-3f), cnt - start);
    int lo = 4095, hi = 0;
    for (int i = 0; i < count; ++i) {
      lo = min(lo, (int)raw[start + i]);
      hi = max(hi, (int)raw[start + i]);
    }
    auto Y = [&](float v) { return 250 - (int)((v - lo) * 60 / max(1, hi - lo)); };
    float yf = raw[start];
    int px = 0, py = 0, pf = 0;
    for (int i = 0; i < count; ++i) {
      yf = a * yf + (1 - a) * raw[start + i];
      const int x = 7 + i * 226 / max(1, count - 1), y = Y(raw[start + i]), yy = Y(yf);
      if (i) {
        tft.drawLine(px, py, x, y, C_DIM);
        tft.drawLine(px, pf, x, yy, C_ACCENT);
      }
      px = x;
      py = y;
      pf = yy;
    }
    textC(260, "z = e^jw: H(z) becomes H(e^jw)", 1, C_DIM);
    page.label = "<  RC";
  }
  drawBtn(page);
  drawBtn(B_TH_BACK);
}

void onMath(int x, int y) {
  if (hit(B_TH_BACK, x, y)) {
    showResult();
  } else if (hit(B_TH_PAGE, x, y)) {
    mathPage = 1 - mathPage;
    showMath();
  }
}

// ---------------- SCENES (several saved keys sent one after the other) ----------------
const Btn B_NEW_S = {5, 276, 112, 40, "+ New"};
const Btn B_BACK_SL = {123, 276, 112, 40, "Back"};
int rowScene[MAX_SCENES];
int sceneRows = 0;
int curScene = -1;
Scene scn;                     // the scene on the SCENE screen

void showScenes() {
  screen = Screen::SCENES;
  header("Scenes");
  sceneRows = 0;
  Scene sc;
  char line[16];
  for (int i = 0; i < MAX_SCENES; ++i) {
    if (!store::loadScene(i, sc)) continue;
    const Btn row = remoteRow(sceneRows);
    rowScene[sceneRows++] = i;
    tft.fillRoundRect(row.x, row.y, row.w, row.h, 6, C_PANEL);
    tft.drawRoundRect(row.x, row.y, row.w, row.h, 6, C_EDGE);
    text(row.x + 10, row.y + 11, sc.name, 2);
    snprintf(line, sizeof line, "%u keys", sc.count);
    text(row.x + row.w - 8 - strlen(line) * 6, row.y + 15, line, 1, C_DIM);
  }
  if (!sceneRows) {
    textC(100, "No scenes yet", 2, C_DIM);
    textC(130, "A scene sends several saved keys", 1, C_DIM);
    textC(144, "with one tap, e.g. TV + AC on", 1, C_DIM);
  }
  const bool full = sceneRows >= MAX_SCENES;
  drawBtn(B_NEW_S, full ? C_OFF : C_ACCENT, full ? C_DIM : C_INK);
  drawBtn(B_BACK_SL);
}

void onScenes(int x, int y) {
  if (hit(B_BACK_SL, x, y)) {
    showHome();
    return;
  }
  if (hit(B_NEW_S, x, y)) {
    const int i = store::freeScene();
    if (i < 0) return;
    Scene sc;
    if (editName("Scene name", sc.name) && store::saveScene(i, sc)) {
      curScene = i;
      scn = sc;
      showScene();
    } else {
      showScenes();
    }
    return;
  }
  for (int r = 0; r < sceneRows; ++r) {
    if (!hit(remoteRow(r), x, y)) continue;
    curScene = rowScene[r];
    if (store::loadScene(curScene, scn)) showScene();
    return;
  }
}

// ---------------- SCENE (its keys, run, edit) ----------------
const Btn B_S_RUN = {5, 248, 112, 32, "Run"};
const Btn B_S_ADD = {123, 248, 112, 32, "+ Key"};
const Btn B_S_REN = {5, 286, 74, 30, "Rename"};
const Btn B_S_DEL = {83, 286, 74, 30, "Delete"};
const Btn B_S_BACK = {161, 286, 74, 30, "Back"};

Btn stepRow(int i) { return {5, (int16_t)(34 + i * 26), 190, 24, ""}; }
Btn stepDel(int i) { return {199, (int16_t)(34 + i * 26), 36, 24, "x"}; }

void drawStep(int i, bool busy) {
  const Btn r = stepRow(i);
  char rn[NAME_LEN] = "?", kn[NAME_LEN] = "?", line[40];
  const bool ok = store::remoteName(scn.remote[i], rn) && store::buttonName(scn.remote[i], scn.key[i], kn);
  if (ok) snprintf(line, sizeof line, "%d  %s  %s", i + 1, rn, kn);
  else snprintf(line, sizeof line, "%d  (key deleted)", i + 1);
  tft.fillRoundRect(r.x, r.y, r.w, r.h, 5, busy ? C_WARN : C_PANEL);
  text(r.x + 6, r.y + 8, line, 1, busy ? C_INK : ok ? C_TEXT : C_DIM);
  drawBtn(stepDel(i), C_PANEL, C_RED);
}

void showScene() {
  screen = Screen::SCENE;
  header(scn.name);
  for (int i = 0; i < scn.count; ++i) drawStep(i, false);
  if (!scn.count) {
    textC(110, "No keys yet", 2, C_DIM);
    textC(140, "Press + Key and pick saved keys", 1, C_DIM);
  }
  drawBtn(B_S_RUN, scn.count ? C_ACCENT : C_OFF, scn.count ? C_INK : C_DIM);
  const bool full = scn.count >= MAX_STEPS;
  drawBtn(B_S_ADD, full ? C_OFF : C_PANEL, full ? C_DIM : C_TEXT);
  drawBtn(B_S_REN);
  drawBtn(B_S_DEL, C_PANEL, C_RED);
  drawBtn(B_S_BACK);
}

void runScene() {
  int sent = 0;
  for (int i = 0; i < scn.count; ++i) {
    drawStep(i, true);
    if (store::loadButton(scn.remote[i], scn.key[i], keySig)) {
      doSend(keySig, false);
      ++sent;
    }
    delay(SCENE_GAP_MS);
    drawStep(i, false);
  }
  Serial.printf("Scene %s: %d of %u keys sent\n", scn.name, sent, scn.count);
}

// Called from the REMOTE screen in Pick::STEP mode.
void addSceneStep(int r, int b) {
  pick = Pick::NONE;
  if (scn.count < MAX_STEPS) {
    scn.remote[scn.count] = r;
    scn.key[scn.count] = b;
    ++scn.count;
    store::saveScene(curScene, scn);
  }
  showScene();
}

void onScene(int x, int y) {
  for (int i = 0; i < scn.count; ++i) {
    if (!hit(stepDel(i), x, y)) continue;
    for (int k = i; k + 1 < scn.count; ++k) {
      scn.remote[k] = scn.remote[k + 1];
      scn.key[k] = scn.key[k + 1];
    }
    --scn.count;
    store::saveScene(curScene, scn);
    showScene();
    return;
  }
  if (hit(B_S_RUN, x, y) && scn.count) {
    drawBtn(B_S_RUN, C_WARN, C_INK);
    runScene();
    drawBtn(B_S_RUN, C_ACCENT, C_INK);
  } else if (hit(B_S_ADD, x, y) && scn.count < MAX_STEPS) {
    pick = Pick::STEP;
    showRemotes();
  } else if (hit(B_S_REN, x, y)) {
    char name[NAME_LEN];
    strcpy(name, scn.name);
    if (editName("Scene name", name)) {
      strcpy(scn.name, name);
      store::saveScene(curScene, scn);
    }
    showScene();
  } else if (hit(B_S_DEL, x, y)) {
    if (confirmBox("Delete scene?")) {
      store::removeScene(curScene);
      curScene = -1;
      showScenes();
    } else {
      showScene();
    }
  } else if (hit(B_S_BACK, x, y)) {
    showScenes();
  }
}

// ---------------- SAMPLING EXPERIMENT (sampling rate, FFT size, aliasing) ----------------
const Btn B_FS[3] = {{5, 34, 74, 36, "50k"}, {83, 34, 74, 36, "200k"}, {161, 34, 74, 36, "500k"}};
const Btn B_N = {5, 76, 230, 30, "N"};
const Btn B_RUN = {5, 276, 112, 40, "Capture"};
const Btn B_BACK_E = {123, 276, 112, 40, "Back"};

void drawExperControls() {
  for (int i = 0; i < 3; ++i) drawBtn(B_FS[i], i == expFsIdx ? C_ACCENT : C_PANEL, i == expFsIdx ? C_INK : C_TEXT);
  Btn n = B_N;
  char lab[16];
  snprintf(lab, sizeof lab, "FFT N = %d", expN);
  n.label = lab;
  drawBtn(n);
}

void drawExperResult() {
  const float fc = (haveCur && cur.carrierHz) ? cur.carrierHz : (float)DEFAULT_CARRIER_HZ;
  const float fsNom = EXP_FS[expFsIdx];
  tft.fillRect(0, 110, SW, 164, C_BG);
  char line[64];
  if (expLen > 0) {
    float* mag = dsp::spectrum(expBuf, expLen);
    const float fpk = dsp::peak(mag, expLen, expFs, 2000.0f, expFs / 2, nullptr);
    drawSpectrum(5, 112, 230, 116, mag, expLen, expFs, fpk);
    snprintf(line, sizeof line, "measured peak %.1f kHz", fpk / 1000);
    textC(232, line, 1);
    snprintf(line, sizeof line, "df = fs/N = %.0f Hz", expFs / expLen);
    textC(244, line, 1, C_DIM);
  } else {
    tft.fillRect(5, 112, 230, 116, C_PANEL);
    textC(160, "Press Capture,", 1, C_DIM);
    textC(174, "then a remote key", 1, C_DIM);
  }
  // w0 Ts in units of pi; discrete-time frequencies repeat every 2 pi, so fold it into 0 .. pi
  const float w = 2 * fc / fsNom;
  float wf = fmodf(w, 2.0f);
  if (wf > 1) wf = 2 - wf;
  if (w < 1)
    snprintf(line, sizeof line, "theory: w0Ts = %.2f pi < pi: %.1f kHz", w, fc / 1000);
  else
    snprintf(line, sizeof line, "w0Ts = %.2f pi -> %.2f pi = %.1f kHz", w, wf, dsp::aliasOf(fc, fsNom) / 1000);
  textC(258, line, 1, C_WARN);
}

void showExper() {
  screen = Screen::EXPER;
  header("Sampling test");
  drawExperControls();
  drawExperResult();
  drawBtn(B_RUN, C_ACCENT, C_INK);
  drawBtn(B_BACK_E);
}

void onExper(int x, int y) {
  for (int i = 0; i < 3; ++i) {
    if (hit(B_FS[i], x, y)) {
      expFsIdx = i;
      expLen = 0;
      drawExperControls();
      drawExperResult();
      return;
    }
  }
  if (hit(B_N, x, y)) {
    expN = expN == 256 ? 1024 : 256;
    expLen = 0;
    drawExperControls();
    drawExperResult();
  } else if (hit(B_RUN, x, y)) {
    tft.fillRect(5, 112, 230, 116, C_PANEL);
    textC(166, "Waiting for a remote (10 s)", 1, C_WARN);
    waitRelease();
    const irrx::Result r = irrx::captureRaw(EXP_FS[expFsIdx], expBuf, expN, 10000, anyTouch);
    if (r == irrx::Result::OK) {
      expLen = expN;
      expFs = irrx::rawFs();
      Serial.printf("Experiment: fs=%.0f N=%d captured (send 'x' to dump)\n", expFs, expLen);
    } else {
      expLen = 0;
      waitRelease();
    }
    drawExperResult();
  } else if (hit(B_BACK_E, x, y)) {
    showHome();
  }
}

// ---------------- THEME (dark / light mode and accent colour) ----------------
const Btn B_DARK = {10, 54, 106, 44, "Dark"};
const Btn B_LIGHT = {124, 54, 106, 44, "Light"};
const Btn B_BACK_T = {5, 276, 230, 40, "Back"};

Btn swatch(int i) { return {(int16_t)(10 + (i % 5) * 46), (int16_t)(136 + (i / 5) * 48), 40, 40, ""}; }   // 2 rows of 5

void showTheme() {
  screen = Screen::THEME;
  header("Theme");
  text(10, 40, "Mode", 1, C_DIM);
  drawBtn(B_DARK, darkMode ? C_ACCENT : C_PANEL, darkMode ? C_INK : C_TEXT);
  drawBtn(B_LIGHT, darkMode ? C_PANEL : C_ACCENT, darkMode ? C_TEXT : C_INK);
  text(10, 120, "Colour", 1, C_DIM);
  for (int i = 0; i < THEME_ACCENTS; ++i) {
    const Btn b = swatch(i);
    tft.fillRoundRect(b.x, b.y, b.w, b.h, 8, accentColor(i));
    if (i == accentIdx) {
      tft.drawRoundRect(b.x - 3, b.y - 3, b.w + 6, b.h + 6, 10, C_TEXT);
      tft.drawRoundRect(b.x - 2, b.y - 2, b.w + 4, b.h + 4, 9, C_TEXT);
    }
  }
  textC(236, ACCENTS[accentIdx].name, 2, C_ACCENT);
  textC(258, "Saved automatically", 1, C_DIM);
  drawBtn(B_BACK_T);
}

void setTheme(bool dark, int accent) {
  darkMode = dark;
  accentIdx = accent;
  applyTheme();
  store::saveTheme(darkMode, accentIdx);
  showTheme();
}

void onTheme(int x, int y) {
  if (hit(B_DARK, x, y)) setTheme(true, accentIdx);
  else if (hit(B_LIGHT, x, y)) setTheme(false, accentIdx);
  else if (hit(B_BACK_T, x, y)) showHome();
  else
    for (int i = 0; i < THEME_ACCENTS; ++i)
      if (hit(swatch(i), x, y)) setTheme(darkMode, i);
}

// ---------------- ABOUT (project credits, also shown while starting) ----------------
const char* const COURSE = "Signals and Systems";
const char* const COURSE_CODE = "EE 3521";
const char* const TEAM[] = {"Talal Aljohani", "Saad Obaid", "Moatasim Ahmad", "Yousef Ahmed"};
const Btn B_BACK_AB = {5, 290, 230, 28, "Back"};

// Signal strip on the start screen: one NEC key press (LG power, 0x04 / 0x08) scrolls in from the right.
// Top row = the light x(t) = m(t) * c(t) (carrier bursts), bottom row = the message m(t) the device recovers.
// Scale: 1 px = 75 us of the real signal; the carrier is drawn 6 px per period (on 1/3), not to scale.
constexpr int WAVE_X = 30, WAVE_Y = 108, WAVE_W = 204, WAVE_H = 38;
uint16_t waveBuf[WAVE_W * WAVE_H];
int16_t waveEdge[72];      // end of each mark / space in px; even index = mark
int waveN = 0, waveLen = 0;

void buildWave() {
  if (waveN) return;
  int t = 0;
  auto add = [&](int d) { t += d; waveEdge[waveN++] = t; };
  add(120); add(60);                             // leader 9 ms + 4.5 ms
  const uint8_t bytes[4] = {0x04, 0xFB, 0x08, 0xF7};
  for (int b = 0; b < 32; ++b) {                 // LSB first: 0 = 562 + 562 us, 1 = 562 + 1687 us
    add(8);
    add((bytes[b / 8] >> (b % 8)) & 1 ? 22 : 7);
  }
  add(8); add(210);                              // stop mark, then a quiet gap before the next press
  waveLen = t;
}

bool waveMark(int p) {
  p %= waveLen;
  if (p < 0) p += waveLen;
  int i = 0;
  while (i < waveN - 1 && p >= waveEdge[i]) ++i;
  return (i & 1) == 0;
}

// off = signal position (px) at the right edge of the strip; it grows with time, so the signal moves left.
void drawWave(int off) {
  buildWave();
  const uint16_t light = C_WARN, line = C_ACCENT, base = C_EDGE;
  for (int i = 0; i < WAVE_W * WAVE_H; ++i) waveBuf[i] = C_BG;
  bool prev = waveMark(off - WAVE_W);
  for (int c = 0; c < WAVE_W; ++c) {
    const int p = off - (WAVE_W - 1) + c;
    const bool m = p >= 0 && waveMark(p);
    const bool on = m && ((p % 6) + 6) % 6 < 2;   // carrier: on for the first third of each period
    for (int r = 2; r < 18; ++r) waveBuf[r * WAVE_W + c] = on ? light : C_BG;
    if (!on) waveBuf[17 * WAVE_W + c] = base;
    const int lvl = m ? 22 : 34;                  // message level: 2 px thick line
    waveBuf[lvl * WAVE_W + c] = line;
    waveBuf[(lvl + 1) * WAVE_W + c] = line;
    if (c > 0 && m != prev)
      for (int r = 22; r < 36; ++r) waveBuf[r * WAVE_W + c] = line;   // edge
    prev = m;
  }
  tft.drawRGBBitmap(WAVE_X, WAVE_Y, waveBuf, WAVE_W, WAVE_H);
}

// Credits layout: the start screen has the signal strip, the About screen does not (more room, Back key at the bottom).
int creditsMadeY = 128, creditsTeamY = 150;      // About positions, used by the easter egg

void drawCredits(bool splash) {
  tft.fillScreen(C_BG);
  if (splash) {
    textC(12, "IR Cloner", 3, C_ACCENT);
    textC(42, "& Analyzer", 2);
    textC(66, COURSE, 2);
    textC(86, COURSE_CODE, 2, C_DIM);
    text(4, WAVE_Y + 6, "x(t)", 1, C_DIM);
    text(4, WAVE_Y + 25, "m(t)", 1, C_DIM);
    drawWave(WAVE_W - 1);
    tft.drawFastHLine(20, 150, SW - 40, C_ACCENT);
    textC(155, "Made by", 2, C_ACCENT);
    for (int i = 0; i < 4; ++i) textC(174 + i * 17, TEAM[i], 2);
    textC(246, "Supervisor", 2, C_ACCENT);
    textC(265, "Dr. Abdulmajeed", 2);
    textC(283, "Alenezi", 2);
  } else {
    textC(14, "IR Cloner", 3, C_ACCENT);
    textC(46, "& Analyzer", 2);
    textC(74, COURSE, 2);
    textC(94, COURSE_CODE, 2, C_DIM);
    tft.drawFastHLine(20, 118, SW - 40, C_ACCENT);
    textC(creditsMadeY, "Made by", 2, C_ACCENT);
    for (int i = 0; i < 4; ++i) textC(creditsTeamY + i * 18, TEAM[i], 2);
    textC(230, "Supervisor", 2, C_ACCENT);
    textC(252, "Dr. Abdulmajeed", 2);
    textC(270, "Alenezi", 2);
  }
}

void showAbout() {
  screen = Screen::ABOUT;
  drawCredits();
  drawBtn(B_BACK_AB);
}

// ---------------- Easter egg: Pulse Runner (tap "Talal Aljohani" 8 times on the About screen) ----------------
// A photon runs along the IR line. Every mark of a key press is a wall: tap the screen to jump over it.
constexpr int EGG_TAPS = 8;
constexpr int G_TOP = 46, G_GROUND = 236, G_PX = 40, G_PS = 10;   // play field, ground line, player x and size
int gameBest = 0;

struct GWall {
  float x;
  int w, h;
};

void gameScore(int score) {
  tft.fillRect(0, 32, SW, 10, C_BG);
  char t[32];
  snprintf(t, sizeof t, "Bits %d (0x%02X)", score, score & 0xFF);
  text(6, 33, t, 1, C_TEXT);
  snprintf(t, sizeof t, "Best %d", gameBest);
  text(SW - 6 - (int)strlen(t) * 6, 33, t, 1, C_DIM);
}

void gameWall(GWall& g, float x) {
  static const uint8_t widths[] = {8, 8, 8, 8, 14, 22};          // mostly short marks, sometimes long ones
  g.x = x;
  g.w = widths[random((long)sizeof widths)];
  g.h = 16 + random(19);
}

void drawWallAt(const GWall& g, int frame) {
  const int x0 = (int)g.x, y0 = G_GROUND - g.h;
  for (int c = 0; c < g.w; ++c) {
    const int sx = x0 + c;
    if (sx < 0 || sx >= SW) continue;
    const bool on = ((c + frame) % 3) != 2;                       // shimmering carrier inside the mark
    tft.drawFastVLine(sx, y0, g.h, on ? C_WARN : C_PANEL);
  }
}

// Returns true when the player wants another round.
bool playRound() {
  header("Pulse Runner");
  gameScore(0);
  tft.drawFastHLine(0, G_GROUND, SW, C_EDGE);
  textC(G_GROUND + 10, "x(t) = m(t) c(t)", 1, C_DIM);
  textC(G_GROUND + 30, "Each wall is a mark of a key press.", 1, C_DIM);
  textC(G_GROUND + 42, "Tap anywhere to jump over it.", 1, C_DIM);
  textC(120, "Tap to start", 2, C_ACCENT);
  waitRelease();
  int x, y;
  while (!touch::read(x, y)) delay(10);
  power::activity();
  tft.fillRect(0, 110, SW, 30, C_BG);
  randomSeed(micros());

  const int NW = 4;
  GWall w[NW];
  float nextX = SW + 20;
  for (int i = 0; i < NW; ++i) {
    gameWall(w[i], nextX);
    nextX += w[i].w + 90 + random(90);
  }
  float py = G_GROUND - G_PS, vy = 0;
  int oldPy = (int)py, score = 0, frame = 0;
  bool wasDown = true;                       // the start tap is still on the screen
  bool alive = true;
  uint32_t next = millis();
  while (alive) {
    next += 25;
    ++frame;
    const float speed = min(7.0f, 3.0f + score / 8.0f);
    const bool down = touch::read(x, y);
    const bool grounded = py >= G_GROUND - G_PS;
    if (down && !wasDown && grounded) vy = -7.8f;
    wasDown = down;
    vy += 0.55f;
    py += vy;
    if (py > G_GROUND - G_PS) {
      py = G_GROUND - G_PS;
      vy = 0;
    }
    power::activity();

    tft.fillRect(G_PX, oldPy, G_PS, G_PS, C_BG);                  // erase the player
    float far = 0;
    for (int i = 0; i < NW; ++i) far = max(far, w[i].x + w[i].w);
    for (int i = 0; i < NW; ++i) {
      GWall& g = w[i];
      const int oldRight = (int)g.x + g.w;
      const bool passed = g.x + g.w < G_PX;
      g.x -= speed;
      const int newRight = (int)g.x + g.w;
      const int ex = max(0, newRight);                            // trailing strip the wall has left
      if (oldRight > ex) tft.fillRect(ex, G_GROUND - g.h, oldRight - ex, g.h, C_BG);
      if (!passed && g.x + g.w < G_PX) {
        ++score;
        if (score > gameBest) gameBest = score;
        gameScore(score);
      }
      if (g.x + g.w < 0) {                                        // off screen: becomes the next wall
        const float gap = 70 + speed * 12 + random(100);
        gameWall(g, max((float)SW, far + gap));
        far = g.x + g.w;
        continue;
      }
      drawWallAt(g, frame);
      // collision (2 px of mercy on each side)
      const int px0 = G_PX + 2, px1 = G_PX + G_PS - 2, py0 = (int)py + 2, py1 = (int)py + G_PS;
      if (px1 > (int)g.x && px0 < (int)g.x + g.w && py1 > G_GROUND - g.h && py0 < G_GROUND) alive = false;
    }
    oldPy = (int)py;
    tft.fillRoundRect(G_PX, oldPy, G_PS, G_PS, 3, alive ? C_ACCENT : C_RED);
    const int32_t wait = (int32_t)(next - millis());
    if (wait > 0) delay(wait);
    else next = millis();
  }

  // game over
  const Btn B_AGAIN = {24, 172, 92, 30, "Again"}, B_QUIT = {124, 172, 92, 30, "Back"};
  tft.fillRoundRect(14, 90, SW - 28, 124, 8, C_PANEL);
  tft.drawRoundRect(14, 90, SW - 28, 124, 8, C_RED);
  textC(102, "Signal lost!", 2, C_RED);
  char t[32];
  snprintf(t, sizeof t, "%d bits received", score);
  textC(128, t, 1, C_TEXT);
  textC(144, score >= gameBest && score > 0 ? "New best!" : "Try again", 1, score >= gameBest && score > 0 ? C_ACCENT : C_DIM);
  drawBtn(B_AGAIN, C_ACCENT, C_INK);
  drawBtn(B_QUIT);
  delay(400);                                                     // a late jump tap must not press a button
  waitRelease();
  while (true) {
    if (getTap(x, y)) {
      if (hit(B_AGAIN, x, y)) return true;
      if (hit(B_QUIT, x, y)) return false;
    }
    delay(15);
  }
}

void runPulseGame() {
  while (playRound()) {
  }
}

int eggTaps = 0;
uint32_t eggLast = 0;

void onAbout(int x, int y) {
  if (hit(B_BACK_AB, x, y)) {
    eggTaps = 0;
    showHome();
    return;
  }
  if (!(x >= 30 && x < 210 && y >= creditsTeamY - 4 && y < creditsTeamY + 18)) return;   // "Talal Aljohani"
  if (millis() - eggLast > 1500) eggTaps = 0;                     // the taps must come quickly
  eggLast = millis();
  if (++eggTaps >= EGG_TAPS) {
    eggTaps = 0;
    runPulseGame();
    showAbout();
    return;
  }
  tft.fillRect(0, creditsMadeY - 1, SW, 18, C_BG);
  if (eggTaps >= 4) {
    char t[24];
    snprintf(t, sizeof t, "%d more...", EGG_TAPS - eggTaps);
    textC(creditsMadeY, t, 2, C_ACCENT);
  } else {
    textC(creditsMadeY, "Made by", 2, C_ACCENT);
  }
}

// ---------------- MONITOR (live photodiode level, for hardware checks) ----------------
// Every 20 ms block: mean, min, max of the raw ADC. A remote key press must make max (and ripple) jump.
void runMonitor(bool fromSerial) {
  header("Light monitor");
  textC(36, "Press remote keys at the IR board", 1, C_DIM);
  textC(48, "Touch the screen to exit", 1, C_DIM);
  const int px0 = 5, py0 = 150, pw = 230, ph = 130;
  tft.fillRect(px0, py0, pw, ph, C_PANEL);
  text(px0 + 3, py0 + 3, "4095", 1, C_DIM);
  text(px0 + 3, py0 + ph - 10, "0", 1, C_DIM);
  if (!fromSerial) {
    waitRelease();
  } else {
    // the Enter after "m" sends \r\n: drop it, or it would stop the monitor immediately
    delay(30);
    while (Serial.available()) Serial.read();
    Serial.println("MON started (60 s). Send any letter to stop.");
  }
  if (!adcStream::start(DECODE_FS)) return;
  adcStream::flush();

  static uint16_t chunk[512];
  const uint32_t blockN = (uint32_t)(adcStream::measuredFs() * 0.02f);   // 20 ms
  int col = 0, blocks = 0;
  const uint32_t tEnd = millis() + 60000;                                 // serial mode: 60 s
  while (true) {
    uint32_t n = 0;
    uint64_t sum = 0;
    int lo = 4095, hi = 0;
    while (n < blockN) {
      const size_t got = adcStream::read(chunk, 512, 50);
      for (size_t i = 0; i < got; ++i) {
        sum += chunk[i];
        lo = min(lo, (int)chunk[i]);
        hi = max(hi, (int)chunk[i]);
      }
      n += got;
      if (!got) break;
    }
    if (!n) continue;
    const int mean = sum / n;

    // scrolling plot: one column per block, min..max
    const int x = px0 + col;
    tft.drawFastVLine(x, py0, ph, C_PANEL);
    const int yHi = py0 + ph - 1 - hi * (ph - 1) / 4095;
    const int yLo = py0 + ph - 1 - lo * (ph - 1) / 4095;
    tft.drawFastVLine(x, yHi, max(1, yLo - yHi + 1), C_ACCENT);
    tft.drawFastVLine(px0 + (col + 1) % pw, py0, ph, C_WARN);       // cursor
    col = (col + 1) % pw;

    if (++blocks % 5 == 0) {                                         // numbers every 100 ms
      char line[48];
      tft.fillRect(0, 62, SW, 84, C_BG);
      snprintf(line, sizeof line, "mean %4d  %.2fV", mean, mean * 3.1f / 4095);
      textC(64, line, 2);
      snprintf(line, sizeof line, "min %4d  max %4d", lo, hi);
      textC(88, line, 2);
      snprintf(line, sizeof line, "ripple %d", hi - lo);
      textC(112, line, 2, C_ACCENT);
      const char* hint = mean > 3900 ? "SATURATED: too much light or R1 too big"
                       : hi < 30     ? "always ~0: check LINK4-IO35 wire / photodiode"
                                     : "OK: max must jump when a key is pressed";
      textC(136, hint, 1, mean > 3900 || hi < 30 ? C_RED : C_DIM);
      Serial.printf("MON mean=%d min=%d max=%d ripple=%d\n", mean, lo, hi, hi - lo);
    }
    if (fromSerial ? (millis() > tEnd || Serial.available()) : anyTouch()) break;
  }
  if (fromSerial) {
    while (Serial.available()) Serial.read();
  } else {
    waitRelease();
  }
  showHome();
}

// ---------------- Touch calibration ----------------
// Raw touch values every 0.5 s on Serial, so wiring problems can be seen:
//   not pressed: z1 ~ 0, z2 ~ 4095    pressed: z1 rises, z2 falls (pressure = z1 + 4095 - z2 > 400)
//   always 0 or always 4095/8191 = no answer from the touch chip (T_CLK D19, T_CS D21, T_DIN D22, T_DO D23)
void printTouchRaw() {
  int z1, z2, x, y;
  touch::lastRaw(z1, z2, x, y);
  Serial.printf("    touch raw: z1=%d z2=%d pressure=%d x=%d y=%d\n", z1, z2, z1 + 4095 - z2, x, y);
}

bool runCalibration() {
  const int sx[3] = {20, SW - 20, 20};
  const int sy[3] = {20, 20, SH - 20};
  int rx[3], ry[3];
  for (int p = 0; p < 3; ++p) {
    tft.fillScreen(C_BG);
    textC(140, "Touch calibration", 2, C_ACCENT);
    char line[40];
    snprintf(line, sizeof line, "Press and hold the cross (%d/3)", p + 1);
    textC(168, line, 1);
    tft.drawFastHLine(sx[p] - 15, sy[p], 31, C_WARN);
    tft.drawFastVLine(sx[p], sy[p] - 15, 31, C_WARN);
    tft.drawCircle(sx[p], sy[p], 8, C_WARN);
    waitRelease();
    long ax = 0, ay = 0;
    int got = 0, x, y;
    const uint32_t t0 = millis();
    uint32_t lastPrint = 0;
    while (got < 16) {
      if (touch::readRaw(x, y)) {
        ax += x;
        ay += y;
        ++got;
      }
      if (millis() - lastPrint > 500) {
        lastPrint = millis();
        printTouchRaw();
      }
      if (millis() - t0 > 30000) {                 // no touch for 30 s: continue without calibration
        Serial.println("touch: no press detected in 30 s, skipping calibration (touch chip silent)");
        TouchCal c;
        c.magic = 0;
        c.ax = SW / 4095.0f;
        c.ay = SH / 4095.0f;
        touch::setCal(c);
        return false;
      }
      delay(10);
    }
    rx[p] = ax / got;
    ry[p] = ay / got;
    Serial.printf("cal point %d: raw %d, %d\n", p, rx[p], ry[p]);
    waitRelease();
  }
  const TouchCal c = touch::solve(sx, sy, rx, ry);
  Serial.printf("touch cal: swap=%d ax=%.4f bx=%.1f ay=%.4f by=%.1f\n", c.swapXY, c.ax, c.bx, c.ay, c.by);
  if (!touch::valid(c)) {
    Serial.println("touch cal REJECTED: readings do not change between corners (touch chip silent)");
    message("Touch not working", "Check T_CLK T_CS T_DIN T_DO", C_RED, 2500);
    return false;
  }
  touch::setCal(c);
  store::saveCal(c);
  return true;
}

// ---------------- Transmitter tests (Serial) ----------------
// b: sends NEC 0x04/0x08 every 300 ms for 10 s. Look at the IR LEDs through a phone camera: they must flash.
void blinkTest() {
  static IrSignal t;
  irdecode::makeNec(0x04, 0x08, t);
  Serial.println("TX blink test: 10 s, watch the IR LEDs through a phone camera");
  for (int i = 0; i < 33; ++i) {
    const bool ok = irtx::send(t);
    if (i == 0) Serial.printf("  first send %s\n", ok ? "OK" : "FAILED");
    delay(300);
  }
  Serial.println("TX blink test done");
}

// t: loop-back. Sends NEC 0x04/0x08 (after 40 ms of silence) while capturing with our own photodiode.
// Put the photodiode in front of the IR LEDs (a few cm). PASS = transmitter + receiver + decoder all work.
void selfTest() {
  static IrSignal t, rx;
  irdecode::makeNec(0x04, 0x08, t);
  Serial.println("TX/RX self-test: photodiode must face the IR LEDs");
  adcStream::start(DECODE_FS);                // (re)starting the ADC takes 0.4 s: do it before the 40 ms lead-in
  if (!irtx::sendAsync(t, 40000)) {
    Serial.println("  FAIL: RMT did not start (see messages above)");
    return;
  }
  const irrx::Result r = irrx::capture(rx, 1500, nullptr);
  while (irtx::busy()) delay(1);
  if (r != irrx::Result::OK) {
    Serial.println("  FAIL: nothing received -> check LEDs / 2N2222 pinout / 5 V on the IR board, or aim");
    return;
  }
  IrDecoded d;
  irdecode::decode(rx, d);
  Serial.printf("  received %u edges: %s | %s\n", rx.n, d.line1, d.line2);
  Serial.print("  DUR");
  for (int i = 0; i < rx.n; ++i) Serial.printf(",%u", rx.us[i]);
  Serial.println();
  const float fc = irrx::estimateCarrier(nullptr);
  Serial.printf("  carrier seen by the photodiode: %.2f kHz -> %s\n", fc / 1000,
                fc > 30000 ? "38 kHz carrier present" : "NO clear carrier: the LEDs may only blink (TV ignores that) or R1 filters it");
  const bool pass = d.proto == IrDecoded::NEC && d.address == 0x04 && d.command == 0x08;
  Serial.println(pass ? "  PASS: sent and received NEC 0x04/0x08" : "  PARTIAL: light received but not decoded as NEC 0x04/0x08");
}

// f: sensor speed test. The LEDs send one 3 ms burst (after 20 ms of darkness) while the ADC records 40 ms.
// Fall time (90% -> 10%) after the burst tells photodiode (fast, < 0.3 ms) from phototransistor / saturation (ms).
// g: receiver diagnostic with a real remote (no need for the LEDs to face the photodiode).
// Records 82 ms after the first light, then reports level, saturation, fall time after the first long mark,
// carrier frequency and the first mark/space durations from a plain 50 % threshold.
void remoteTest() {
  // big buffers on the heap (static RAM is too small), freed at the end
  uint16_t* buf = (uint16_t*)malloc(16384 * 2);
  uint16_t* env = (uint16_t*)malloc(16384 / 2 * 2);
  uint16_t* tmp = (uint16_t*)malloc(16384 / 2 * 2);
  struct Free { void* a; void* b; void* c; ~Free() { free(a); free(b); free(c); } } freer{buf, env, tmp};
  if (!buf || !env || !tmp) { Serial.println("  not enough memory"); return; }
  Serial.println("Remote test: point a TV remote at the photodiode (10-30 cm) and press one key (10 s)...");
  const irrx::Result r = irrx::captureRaw(DECODE_FS, buf, 16384, 10000, nullptr);
  if (r != irrx::Result::OK) {
    Serial.println(r == irrx::Result::TIMEOUT ? "  nothing seen in 10 s: the sensor does not react at all (wiring / polarity / LINK4-IO35)"
                                              : "  ADC error");
    return;
  }
  const float fs = irrx::rawFs();
  const int n = 16384, W = max(2, (int)lroundf(fs * 50e-6f)), nw = n / W;
  for (int w = 0; w < nw; ++w) {
    int m = 0;
    for (int k = 0; k < W; ++k) m = max(m, (int)buf[w * W + k]);
    env[w] = m;
  }
  // base = 10th percentile of the envelope, peak = max
  memcpy(tmp, env, nw * 2);
  std::sort(tmp, tmp + nw);
  const float base = tmp[nw / 10], peak = tmp[nw - 1], span = peak - base;
  Serial.printf("  dark level %.0f, peak %.0f counts%s, span %.0f\n", base, peak, peak >= 4090 ? " (SATURATED: move the remote further away)" : "", span);
  if (span < 60) {
    Serial.println("  signal too weak: closer, or check the photodiode / R1");
    return;
  }
  // durations from a 50 % threshold on the envelope
  const float thr = base + 0.5f * span;
  Serial.print("  durations (us, mark first):");
  bool hi = env[0] > thr;
  int start = 0, shown = 0, firstMarkEnd = -1, firstMarkLen = 0;
  for (int w = 1; w < nw && shown < 24; ++w) {
    const bool h = env[w] > thr;
    if (h == hi) continue;
    const int us = lroundf((w - start) * W * 1e6f / fs);
    Serial.printf(" %d", us);
    if (hi && firstMarkEnd < 0 && us > 1000) { firstMarkEnd = w; firstMarkLen = us; }
    hi = h; start = w; ++shown;
  }
  Serial.println();
  Serial.println("  expected NEC: 9000 4500 562 562 562 1687 ... (a slow sensor merges them into a few long marks)");
  // fall time after the first long mark
  if (firstMarkEnd > 0) {
    int i90 = firstMarkEnd * W;
    while (i90 > 0 && buf[i90] < base + 0.9f * span) --i90;
    int i10 = -1;
    for (int w = firstMarkEnd; w < nw; ++w)
      if (env[w] < base + 0.1f * span) { i10 = w * W; break; }
    if (i10 > 0) {
      const float fall = (i10 - i90) * 1e6f / fs;
      Serial.printf("  first long mark %d us, fall time 90%%->10%% after it: %.0f us -> %s\n", firstMarkLen, fall,
                    fall < 300 ? "FAST (photodiode behaviour) - good" : fall < 1000 ? "borderline" : "SLOW (phototransistor or saturated)");
    } else {
      Serial.println("  the level never fell back below 10 % after the first mark: very slow sensor or saturated");
    }
  } else {
    Serial.println("  no mark longer than 1 ms found (no NEC leader, or the signal never fell below 50 %)");
  }
  // carrier from the samples just after the trigger (inside the first mark)
  const int nf = 1024;
  float* mag = dsp::spectrum(buf, nf);
  float snr = 0;
  const float fc = dsp::peak(mag, nf, fs, 25000.0f, 60000.0f, &snr);
  Serial.printf("  carrier in the first mark: %.2f kHz, peak %.0f x the average bin -> %s\n", fc / 1000, powf(10, snr / 20),
                snr >= 8 ? "38 kHz carrier visible" : "carrier not visible (filtered by R1 x C or a slow sensor)");
}

void speedTest() {
  uint16_t* buf = (uint16_t*)malloc(8192 * 2);
  struct Free { void* a; ~Free() { free(a); } } freer{buf};
  if (!buf) { Serial.println("  not enough memory"); return; }
  static IrSignal burst;
  burst.n = 1; burst.us[0] = 3000; burst.carrierHz = 38000;
  Serial.println("Sensor speed test: the photodiode must see the IR LEDs (direct, or a white paper 2-3 cm above both)");
  if (!adcStream::start(DECODE_FS)) return;
  const float fs = adcStream::measuredFs();
  adcStream::flush();
  irtx::sendAsync(burst, 20000);
  int n = 0;
  while (n < 8192) {
    const size_t got = adcStream::read(buf + n, min<size_t>(512, 8192 - n), 50);
    if (!got) break;
    n += got;
  }
  while (irtx::busy()) delay(1);
  const int base_n = (int)(fs * 0.012f);                 // first 12 ms: dark
  long sum = 0;
  for (int i = 0; i < base_n; ++i) sum += buf[i];
  const float base = (float)sum / base_n;
  int ipk = 0;
  for (int i = 0; i < n; ++i) if (buf[i] > buf[ipk]) ipk = i;
  const float peak = buf[ipk], span = peak - base;
  Serial.printf("  dark level %.0f, peak %.0f counts%s\n", base, peak, peak >= 4095 ? " (SATURATED)" : "");
  if (span < 60) {
    Serial.println("  the sensor did not see the burst: point it at the LEDs or use a white paper as a mirror");
    return;
  }
  // last sample above 90 % of the step, then first sample below 10 % after it (skip the carrier ripple: 40 us windows)
  int i90 = -1;
  for (int i = 0; i < n; ++i) if (buf[i] > base + 0.9f * span) i90 = i;
  int i10 = -1;
  const int w = max(2, (int)(fs * 40e-6f));
  for (int i = i90; i + w < n; ++i) {
    int mx = 0;
    for (int k = 0; k < w; ++k) mx = max(mx, (int)buf[i + k]);
    if (mx < base + 0.1f * span) { i10 = i; break; }
  }
  if (i10 < 0) {
    Serial.println("  signal never fell below 10 % within the record: VERY slow sensor (phototransistor?) or still saturated");
    return;
  }
  const float fallUs = (i10 - i90) * 1e6f / fs;
  Serial.printf("  fall time 90%%->10%%: %.0f us -> %s\n", fallUs,
                fallUs < 300 ? "fast: behaves like a photodiode, OK for NEC (562 us bits)"
                : fallUs < 1000 ? "borderline: lower R1 (100k) to speed it up"
                : "SLOW: phototransistor or saturated photodiode - lower R1 or replace the sensor");
}

// ---------------- Serial ----------------
void printSignal(const IrSignal& s) {
  Serial.printf("SIGNAL n=%u carrier=%lu Hz | %s | %s\n", s.n, (unsigned long)s.carrierHz, dec.line1, dec.line2);
  Serial.print("DUR");
  for (int i = 0; i < s.n; ++i) Serial.printf(",%u", s.us[i]);
  Serial.println();
}

void dumpSamples(const char* tag, const uint16_t* x, int n, float fs, int trig) {
  Serial.printf("%s fs=%.1f n=%d trig=%d\n", tag, fs, n, trig);
  for (int i = 0; i < n; ++i) Serial.println(x[i]);
  Serial.printf("END %s\n", tag);
}

void serialCommands() {
  if (!Serial.available()) return;
  const char c = Serial.read();
  switch (c) {
    case 'c': doCapture(true); break;
    case 's':
      if (haveCur) doSend(cur);
      break;
    case 'r':
      if (rawValid) dumpSamples("RAW", irrx::raw(), irrx::rawCount(), irrx::rawFs(), irrx::rawTrigger());
      break;
    case 'x':
      if (expLen) dumpSamples("EXP", expBuf, expLen, expFs, 0);
      break;
    case 'm': runMonitor(true); break;
    case 'b': blinkTest(); break;
    case 'f': speedTest(); break;
    case 'g': remoteTest(); break;
    case 'l': {   // LG TV power (NEC address 0x04, command 0x08): tests the transmitter without any capture
      static IrSignal lg;
      irdecode::makeNec(0x04, 0x08, lg);
      Serial.println("Sending LG power (NEC 0x04 / 0x08)");
      doSend(lg);
      break;
    }
    case 't': selfTest(); break;
    case 'z': {   // receiver RC of the current capture (same numbers as the Math screen)
      const RcFit f = fitRc();
      if (!f.ok) Serial.println("Math: no clean fall after the first mark (capture with c first)");
      else Serial.printf("Math: RC %.1f us, w = 1/RC at %.0f Hz, fall 90-10 %.0f us, dark %.0f, mark level %.0f\n", f.tau * 1e6f,
                         1 / (2 * PI * f.tau), (f.i10 - f.i90) / irrx::rawFs() * 1e6f, f.base, f.top);
      break;
    }
    case 'k':
      runCalibration();
      showHome();
      break;
    case 'h':
      Serial.println("c=capture  s=send  r=dump raw ADC  x=dump experiment  m=light monitor  k=touch calibration");
      Serial.println("b=TX blink test (phone camera)  t=TX->RX self-test (photodiode facing the LEDs)  l=send LG TV power  f=sensor speed test  g=receiver test with a TV remote  z=sensor RC (after c)");
      break;
    default: break;
  }
}

const char* resetReasonText() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "power on";
    case ESP_RST_SW: return "software restart";
    case ESP_RST_PANIC: return "CRASH (panic)";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_BROWNOUT: return "BROWNOUT (supply voltage dropped)";
    case ESP_RST_EXT: return "reset button";
    default: return "other";
  }
}

// ---------------- Arduino entry points (called from IR_Cloner.ino) ----------------
void appSetup() {
  irtx::begin();                       // first: IR LEDs off
  Serial.begin(115200);
  delay(200);
  Serial.printf("\nIR Cloner & Analyzer  (reset reason %d: %s)\n", (int)esp_reset_reason(), resetReasonText());
  Serial.println("Signals and Systems EE 3521 - Talal Aljohani, Saad Obaid, Moatasim Ahmad, Yousef Ahmed");
  Serial.println("Supervisor: Dr. Abdulmajeed Alenezi");

  Serial.println("[1] display");
  applyTheme();                        // default colours until the saved theme is read from flash
  for (int p : {PIN_RGB_R, PIN_RGB_G, PIN_RGB_B, PIN_AUDIO_EN}) {   // on-board LED and amplifier stay off
    pinMode(p, OUTPUT);
    digitalWrite(p, HIGH);
  }
  power::begin();                      // backlight on PWM, dims when nobody touches the screen
  SPI.begin(PIN_TFT_SCLK, PIN_TFT_MISO, PIN_TFT_MOSI, PIN_TFT_CS);
  tft.begin(TFT_SPI_HZ);
  tft.setRotation(TFT_ROTATION);
  tft.invertDisplay(TFT_INVERT);
  // Read back display registers: 0x00 or 0xFF everywhere = no communication.
  Serial.printf("    display: power mode 0x%02X, MADCTL 0x%02X, pixel format 0x%02X, self-diag 0x%02X\n",
                tft.readcommand8(ILI9341_RDMODE), tft.readcommand8(ILI9341_RDMADCTL),
                tft.readcommand8(ILI9341_RDPIXFMT), tft.readcommand8(ILI9341_RDSELFDIAG));
  tft.fillScreen(C_BG);

  Serial.println("[2] touch");
  touch::begin();
  Serial.println("[3] flash (first boot formats it, a few seconds)");
  if (!store::begin()) Serial.println("LittleFS mount failed");
  if (store::loadTheme(darkMode, accentIdx)) applyTheme();
  drawCredits(true);                   // start screen, in the saved theme
  const uint32_t splash = millis();
  power::batteryMv(true);              // before the IR stream starts: nothing to pause yet
  Serial.printf("    battery %d mV (%d %%)\n", power::batteryMv(false), power::percent(power::batteryMv(false)));
  Serial.println("[4] ADC");
  if (!adcStream::start(DECODE_FS)) Serial.println("ADC start failed");

  // The start screen stays SPLASH_MS. A finger on it meanwhile forces a new touch calibration
  // (the Touch cal key may be unreachable when the calibration is bad).
  textC(308, "Hold the screen to redo touch cal", 1, C_DIM);
  bool held = false;
  int rx, ry;
  while (!held && millis() - splash < SPLASH_MS) {
    held = touch::readRaw(rx, ry);
    drawWave(WAVE_W - 1 + (int)((millis() - splash) / 3));   // the key press scrolls on at 1 px per 3 ms
    delay(10);
  }
  TouchCal c;
  if (!held && store::loadCal(c) && touch::valid(c)) {
    touch::setCal(c);
  } else {
    Serial.println(held ? "[5] screen held: new touch calibration" : "[5] touch calibration: touch the 3 crosses on the screen");
    runCalibration();
  }
  Serial.println("[6] ready  (send h for serial commands)");
  showHome();
}

void appLoop() {
  if (Serial.available()) power::activity();
  serialCommands();
  power::update();
  static uint32_t lastBattery = 0;
  if (millis() - lastBattery > BAT_REFRESH_MS) {
    lastBattery = millis();
    power::batteryMv(true);
    drawBattery();
  }
  const bool wasAsleep = power::asleep();
  int x, y;
  if (!getTap(x, y)) {
    delay(15);
    return;
  }
  if (wasAsleep) return;                 // the touch that wakes the screen does nothing else
  switch (screen) {
    case Screen::HOME: onHome(x, y); break;
    case Screen::RESULT: onResult(x, y); break;
    case Screen::ANALYZE: onAnalyze(x, y); break;
    case Screen::REMOTES: onRemotes(x, y); break;
    case Screen::REMOTE: onRemote(x, y); break;
    case Screen::KEY: onKeyMenu(x, y); break;
    case Screen::EXPER: onExper(x, y); break;
    case Screen::THEME: onTheme(x, y); break;
    case Screen::ABOUT: onAbout(x, y); break;
    case Screen::STEPS: onSteps(x, y); break;
    case Screen::MATH: onMath(x, y); break;
    case Screen::SCENES: onScenes(x, y); break;
    case Screen::SCENE: onScene(x, y); break;
  }
}
