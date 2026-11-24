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
};
static constexpr int MODE_COUNT = sizeof(modes) / sizeof(modes[0]);
static int currentMode = 0;

const char *currentModeName() {
    return modes[currentMode]->name();
}

// Restores the mode used before the restart / power-off
static void enterStoredMode() {
    ModeId ids[MODE_COUNT];
    for (int i = 0; i < MODE_COUNT; i++) {
        ids[i] = modes[i]->id();
    }
    currentMode = findModeIndex(ids, MODE_COUNT, Config.mode());
    MAIN_DEBUG("Entering mode " + String(modes[currentMode]->name()));
    hw->redrawRequested = true;
    modes[currentMode]->enter(*hw);
}

static void switchToNextMode() {
    int next = nextModeIndex(currentMode, MODE_COUNT);
    if (next == currentMode) return;  // a single mode in the build
    modes[currentMode]->leave(*hw);
    currentMode = next;
    MAIN_DEBUG("Switching to mode " + String(modes[currentMode]->name()));
    Config.setMode(modes[currentMode]->id());
    hw->redrawRequested = false;  // the mode draws when its name finishes
    hw->showModeName(modes[currentMode]->name());
    modes[currentMode]->enter(*hw);
}

// Long Mid press: config page for 5 minutes, its address scrolls once
static void startConfigPage() {
    MAIN_DEBUG("Starting config server.");
    Config.startConfigServer();
    hw->showConfigServerAddress();
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
    if (!hw->ensureConnection()) {
        MAIN_DEBUG("Connection not established, rebooting in 5 seconds...");
        delay(5000);
        hw->reboot();
    }
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
