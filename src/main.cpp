// Round countdown clock firmware for Ulanzi TC001 desktop clock hardware,
// fetching the data from www.bestcoastpairings.com
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include <buildfeatures.h>
#include <Arduino.h>
#include <WiFi.h>
#include <config.h>
#include <hw.h>
#include <tc001.h>
#include <devlog.h>
#include "mode.h"
#if BCP_MODE_BCP
#include "bcpmode.h"
#endif
#if BCP_MODE_OFFLINE
#include "offlinemode.h"
#endif
#if BCP_MODE_CHESS
#include "chessmode.h"
#endif
#ifdef BCP_DEV
#include "devconsole.h"
#endif

#define MAIN_DEBUG(msg) DEV_LOG("Main", msg)

Hw *hw;

// ---- Modes ----

// The modes of the build in the order of switching (short Mid press)
static Mode *const modes[] = {
#if BCP_MODE_BCP
    &bcpMode,
#endif
#if BCP_MODE_OFFLINE
    &offlineMode,
#endif
#if BCP_MODE_CHESS
    &chessMode,
#endif
};
static constexpr int MODE_COUNT = sizeof(modes) / sizeof(modes[0]);
static int currentMode = 0;

const char *currentModeName() {
    return modes[currentMode]->name();
}

// The first mode from index on (index included) that works without a network, -1 if none
static int findModeWithoutNetwork(int index) {
    for (int i = 0; i < MODE_COUNT; i++) {
        int candidate = (index + i) % MODE_COUNT;
        if (!modes[candidate]->needsNetwork()) return candidate;
    }
    return -1;
}

// Connects to WiFi for a mode that needs it; without a connection returns a mode that works without it,
// or reboots if the build has none. A short Mid press leaves the captive portal to that mode.
static int connectForMode(int index) {
    if (!modes[index]->needsNetwork() || hw->isConnected()) return index;
    int fallback = findModeWithoutNetwork(index);
    if (hw->ensureConnection(true, fallback >= 0)) return index;
    if (fallback >= 0) {
        MAIN_DEBUG("No WiFi, continuing in mode " + String(modes[fallback]->name()));
        return fallback;
    }
    MAIN_DEBUG("Connection not established, rebooting in 5 seconds...");
    delay(5000);
    hw->reboot();
    return index;
}

// Restores the mode used before the restart / power-off
static void enterStoredMode() {
    ModeId ids[MODE_COUNT];
    for (int i = 0; i < MODE_COUNT; i++) {
        ids[i] = modes[i]->id();
    }
    int index = findModeIndex(ids, MODE_COUNT, Config.mode());
    if (modes[index]->needsNetwork()) {
        index = connectForMode(index);
    } else {
        hw->ensureConnection(false, true);   // stored networks only (portal if Mid held), works without WiFi too
    }
    currentMode = index;
    Config.setMode(modes[currentMode]->id());
    MAIN_DEBUG("Entering mode " + String(modes[currentMode]->name()));
    hw->redrawRequested = true;
    modes[currentMode]->enter(*hw);
}

static void switchToNextMode() {
    int next = nextModeIndex(currentMode, MODE_COUNT);
    if (next == currentMode) return;  // a single mode in the build
    modes[currentMode]->leave(*hw);
    currentMode = connectForMode(next);
    MAIN_DEBUG("Switching to mode " + String(modes[currentMode]->name()));
    Config.setMode(modes[currentMode]->id());
    hw->redrawRequested = false;  // the mode draws when its name finishes
    hw->showModeName(modes[currentMode]->name());
    modes[currentMode]->enter(*hw);
}

// Long Mid press: config page for 5 minutes, its address scrolls once
static void startConfigPage() {
#if BCP_FEATURE_WIFIMGR
    if (hw->isConnected()) {
        MAIN_DEBUG("Starting config server.");
        Config.startConfigServer();
        hw->showConfigServerAddress();
        return;
    }
#endif
    hw->redrawRequested = false;
    hw->showModeName("No WiFi");  // a notice shown like a mode name
}

static void handleButtons() {
    for (ButtonEvent event = hw->nextButtonEvent(); event != BUTTON_NONE; event = hw->nextButtonEvent()) {
        if (event == BUTTON_MID_LONG) {
            startConfigPage();
        } else if (modes[currentMode]->onButton(*hw, event) == MODE_NEXT) {
            switchToNextMode();
        }
    }
}

// Applies a configuration saved on the config page
static void handleConfigUpdate() {
    if (!Config.configUpdated) return;
    MAIN_DEBUG("Configuration updated.");
    Config.configUpdated = false;
#if BCP_FEATURE_WIFIMGR
    WiFi.setHostname(Config.hostname().c_str());   // DHCP hostname applies at the next WiFi connection
#endif
    modes[currentMode]->onConfigUpdated(*hw);
}

// ---- core Arduino functions ----

void setup() {
    hw = &Tc001::getInstance();
    hw->showSplashScreen();
#if BCP_FEATURE_WIFIMGR
    WiFi.setHostname(Config.hostname().c_str());   // must be set before WiFi starts
#endif
    // Config.erase(); // for testing only - remove in production
    enterStoredMode();
}

void loop() {
    handleConfigUpdate();
    handleButtons();
    modes[currentMode]->tick(*hw);

#ifdef BCP_DEV
    tickDevConsole(hw);
#endif
    hw->tick();
    delay(10);
}
