// Saved remotes and touch calibration in flash (LittleFS).
// A remote has a name and up to MAX_BUTTONS buttons; each button has a name and one captured signal.
#pragma once
#include "ir_types.h"
#include "touch_xpt2046.h"

// A scene sends several saved keys one after the other (for example TV on + AC on).
struct Scene {
  char name[NAME_LEN] = "";
  uint8_t count = 0;
  uint8_t remote[MAX_STEPS] = {};
  uint8_t key[MAX_STEPS] = {};
};

namespace store {
bool begin();                            // also moves signals saved by older firmware into a remote "Old"

bool remoteExists(int r);
bool remoteName(int r, char* name);      // name: NAME_LEN bytes
bool saveRemoteName(int r, const char* name);
bool removeRemote(int r);                // with all its buttons
int freeRemote();                        // -1 when MAX_REMOTES are used
int buttonCount(int r);

bool buttonExists(int r, int b);
bool buttonName(int r, int b, char* name);
bool saveButton(int r, int b, const char* name, const IrSignal& sig);
bool loadButton(int r, int b, IrSignal& sig);
bool renameButton(int r, int b, const char* name);
bool removeButton(int r, int b);
int freeButton(int r);                   // -1 when the remote is full

bool sceneExists(int i);
bool loadScene(int i, Scene& sc);
bool saveScene(int i, const Scene& sc);
bool removeScene(int i);
int freeScene();                         // -1 when MAX_SCENES are used

bool saveCal(const TouchCal& c);
bool loadCal(TouchCal& c);
bool saveTheme(bool dark, int accent);
bool loadTheme(bool& dark, int& accent);   // false: keep the defaults
}
