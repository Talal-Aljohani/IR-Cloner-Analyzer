#include "storage.h"
#include <LittleFS.h>

namespace {
constexpr uint32_t SIG_MAGIC = 0x49525331;   // "IRS1": single signal of the older firmware (/sig<i>.bin)
constexpr uint32_t BTN_MAGIC = 0x49524231;   // "IRB1": one button = header + name + durations
constexpr int OLD_SLOTS = 6;
constexpr uint32_t SCENE_MAGIC = 0x53434E31;   // "SCN1": magic + Scene

struct Header {
  uint32_t magic;
  uint32_t carrierHz;
  uint16_t n;
  uint16_t reserved;
};

// /rem<r>.txt holds the remote name, /rem<r>_<b>.bin one button
String remotePath(int r) { return "/rem" + String(r) + ".txt"; }
String buttonPath(int r, int b) { return "/rem" + String(r) + "_" + String(b) + ".bin"; }
String scenePath(int i) { return "/scene" + String(i) + ".bin"; }

void copyName(char* dst, const char* src) {
  strncpy(dst, src, NAME_LEN - 1);
  dst[NAME_LEN - 1] = 0;
}

// Old firmware stored up to 6 loose signals: keep them as buttons of a remote called "Old".
void migrateSlots() {
  int r = -1;
  for (int i = 0; i < OLD_SLOTS; ++i) {
    const String p = "/sig" + String(i) + ".bin";
    if (!LittleFS.exists(p)) continue;
    if (r < 0) {
      r = store::freeRemote();
      if (r < 0 || !store::saveRemoteName(r, "Old")) return;
    }
    File in = LittleFS.open(p, "r");
    Header h;
    const bool okHeader = in && in.read((uint8_t*)&h, sizeof h) == sizeof h && h.magic == SIG_MAGIC && h.n <= MAX_DURATIONS;
    const int b = store::freeButton(r);
    if (okHeader && b >= 0) {
      File out = LittleFS.open(buttonPath(r, b), "w");
      char name[NAME_LEN] = {};
      snprintf(name, sizeof name, "Signal %d", i + 1);
      h.magic = BTN_MAGIC;
      bool ok = out && out.write((const uint8_t*)&h, sizeof h) == sizeof h && out.write((const uint8_t*)name, NAME_LEN) == NAME_LEN;
      uint8_t chunk[128];
      size_t left = h.n * 2u;
      while (ok && left) {
        const size_t k = in.read(chunk, min(left, sizeof chunk));
        ok = k > 0 && out.write(chunk, k) == k;
        left -= k;
      }
      out.close();
      if (!ok) LittleFS.remove(buttonPath(r, b));
    }
    in.close();
    LittleFS.remove(p);
  }
}
}  // namespace

namespace store {

bool begin() {
  if (!LittleFS.begin(true)) return false;   // formats the partition on first use
  migrateSlots();
  return true;
}

bool remoteExists(int r) { return LittleFS.exists(remotePath(r)); }

bool remoteName(int r, char* name) {
  File f = LittleFS.open(remotePath(r), "r");
  if (!f) return false;
  const size_t k = f.read((uint8_t*)name, NAME_LEN - 1);
  name[k] = 0;
  f.close();
  return k > 0;
}

bool saveRemoteName(int r, const char* name) {
  File f = LittleFS.open(remotePath(r), "w");
  if (!f) return false;
  char buf[NAME_LEN];
  copyName(buf, name);
  const size_t k = strlen(buf);
  const bool ok = f.write((const uint8_t*)buf, k) == k;
  f.close();
  return ok;
}

bool removeRemote(int r) {
  for (int b = 0; b < MAX_BUTTONS; ++b)
    if (buttonExists(r, b)) LittleFS.remove(buttonPath(r, b));
  return LittleFS.remove(remotePath(r));
}

int freeRemote() {
  for (int r = 0; r < MAX_REMOTES; ++r)
    if (!remoteExists(r)) return r;
  return -1;
}

int buttonCount(int r) {
  int n = 0;
  for (int b = 0; b < MAX_BUTTONS; ++b) n += buttonExists(r, b);
  return n;
}

bool buttonExists(int r, int b) { return LittleFS.exists(buttonPath(r, b)); }

bool buttonName(int r, int b, char* name) {
  File f = LittleFS.open(buttonPath(r, b), "r");
  if (!f) return false;
  Header h;
  const bool ok = f.read((uint8_t*)&h, sizeof h) == sizeof h && h.magic == BTN_MAGIC &&
                  f.read((uint8_t*)name, NAME_LEN) == NAME_LEN;
  name[NAME_LEN - 1] = 0;
  f.close();
  return ok;
}

bool saveButton(int r, int b, const char* name, const IrSignal& sig) {
  File f = LittleFS.open(buttonPath(r, b), "w");
  if (!f) return false;
  const Header h = {BTN_MAGIC, sig.carrierHz, sig.n, 0};
  char buf[NAME_LEN] = {};
  copyName(buf, name);
  bool ok = f.write((const uint8_t*)&h, sizeof h) == sizeof h;
  ok = ok && f.write((const uint8_t*)buf, NAME_LEN) == NAME_LEN;
  ok = ok && f.write((const uint8_t*)sig.us, sig.n * 2) == sig.n * 2u;
  f.close();
  return ok;
}

bool loadButton(int r, int b, IrSignal& sig) {
  File f = LittleFS.open(buttonPath(r, b), "r");
  if (!f) return false;
  Header h;
  char name[NAME_LEN];
  bool ok = f.read((uint8_t*)&h, sizeof h) == sizeof h && h.magic == BTN_MAGIC && h.n <= MAX_DURATIONS &&
            f.read((uint8_t*)name, NAME_LEN) == NAME_LEN;
  if (ok) {
    sig.carrierHz = h.carrierHz;
    sig.n = h.n;
    ok = f.read((uint8_t*)sig.us, h.n * 2) == h.n * 2u;
  }
  f.close();
  return ok;
}

bool renameButton(int r, int b, const char* name) {
  File f = LittleFS.open(buttonPath(r, b), "r+");   // the name sits right after the header: overwrite it in place
  if (!f) return false;
  char buf[NAME_LEN] = {};
  copyName(buf, name);
  const bool ok = f.seek(sizeof(Header)) && f.write((const uint8_t*)buf, NAME_LEN) == NAME_LEN;
  f.close();
  return ok;
}

bool removeButton(int r, int b) { return LittleFS.remove(buttonPath(r, b)); }

int freeButton(int r) {
  for (int b = 0; b < MAX_BUTTONS; ++b)
    if (!buttonExists(r, b)) return b;
  return -1;
}

bool sceneExists(int i) { return LittleFS.exists(scenePath(i)); }

bool loadScene(int i, Scene& sc) {
  File f = LittleFS.open(scenePath(i), "r");
  if (!f) return false;
  uint32_t magic = 0;
  const bool ok = f.read((uint8_t*)&magic, 4) == 4 && magic == SCENE_MAGIC && f.read((uint8_t*)&sc, sizeof sc) == sizeof sc &&
                  sc.count <= MAX_STEPS;
  sc.name[NAME_LEN - 1] = 0;
  f.close();
  return ok;
}

bool saveScene(int i, const Scene& sc) {
  File f = LittleFS.open(scenePath(i), "w");
  if (!f) return false;
  const uint32_t magic = SCENE_MAGIC;
  const bool ok = f.write((const uint8_t*)&magic, 4) == 4 && f.write((const uint8_t*)&sc, sizeof sc) == sizeof sc;
  f.close();
  return ok;
}

bool removeScene(int i) { return LittleFS.remove(scenePath(i)); }

int freeScene() {
  for (int i = 0; i < MAX_SCENES; ++i)
    if (!sceneExists(i)) return i;
  return -1;
}

bool saveCal(const TouchCal& c) {
  File f = LittleFS.open("/touch.cal", "w");
  if (!f) return false;
  const bool ok = f.write((const uint8_t*)&c, sizeof c) == sizeof c;
  f.close();
  return ok;
}

bool loadCal(TouchCal& c) {
  File f = LittleFS.open("/touch.cal", "r");
  if (!f) return false;
  const bool ok = f.read((uint8_t*)&c, sizeof c) == sizeof c && c.magic == touch::CAL_MAGIC;
  f.close();
  return ok;
}

bool saveTheme(bool dark, int accent) {
  File f = LittleFS.open("/theme.cfg", "w");
  if (!f) return false;
  const uint8_t d[4] = {'T', 'H', (uint8_t)dark, (uint8_t)accent};
  const bool ok = f.write(d, sizeof d) == sizeof d;
  f.close();
  return ok;
}

bool loadTheme(bool& dark, int& accent) {
  File f = LittleFS.open("/theme.cfg", "r");
  if (!f) return false;
  uint8_t d[4];
  const bool ok = f.read(d, sizeof d) == sizeof d && d[0] == 'T' && d[1] == 'H' && d[3] < THEME_ACCENTS;
  f.close();
  if (ok) {
    dark = d[2] != 0;
    accent = d[3];
  }
  return ok;
}

}  // namespace store
