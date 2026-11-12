// Round countdown clock firmware for Ulanzi TC001 desktop clock hardware,
// fetching the data from www.bestcoastpairings.com
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include <Arduino.h>
#include <WiFi.h>
#include <config.h>
#include <bcpevent.h>
#include <hw.h>
#include <tc001.h>
#ifdef BCP_DEV
#include "devconsole.h"
#endif

#define DATA_REFRESH_INTERVAL_MS 60000 // 60 seconds
#define MAX_CONNECTION_FAILURES 5      // reboot after this many consecutive connection errors (stuck TLS/HTTP)

Print *debugOut_ = nullptr;  // Optional debug output
#define MAIN_DEBUG(msg) if (debugOut_) { debugOut_->print("[Main] "); debugOut_->println(msg); }

Hw *hw;

// ---- update display after data refresh ----
// The display state is set by the Hw implementation; here it is only read to avoid redrawing
// what is already shown.
void displayUpdate() {
    bool redraw = hw->redrawRequested;
    hw->redrawRequested = false;
    MAIN_DEBUG("Updating display, current state: " + String(displayStateName(hw->displayState)));

    if (hw->displayState == DISPLAY_CONFIG_MESSAGE && !redraw) {
        return;  // config page address is scrolling, eventHandler requests a redraw when it finishes
    }

    static bool wasInvalidEvent = false;

    if (BCPEvent.valid()) {
        MAIN_DEBUG("BCP event is valid.");

        if (wasInvalidEvent) {
            if (Config.isConfigServerRunning()) {
                // restart config server with timeout
                Config.stopConfigServer();
                Config.startConfigServer();
            }
            wasInvalidEvent = false;
        }

        bool isEventRunning = BCPEvent.started() && !BCPEvent.ended();
        if (!isEventRunning) {
            // event not running, show event name
            static String eventName = "";
            if (hw->displayState != DISPLAY_EVENT_NAME || eventName != BCPEvent.name()) {
                MAIN_DEBUG("Displaying event name.");
                hw->displayEventName(BCPEvent);
                eventName = BCPEvent.name();
            }

        } else if (BCPEvent.timerLength() <= 0) {
            // event running but no timer active, show round only
            static int lastRound = -1;
            if (hw->displayState != DISPLAY_EVENT_ROUND || lastRound != BCPEvent.currentRound()) {
                MAIN_DEBUG("Displaying event round.");
                hw->displayEventRound(BCPEvent);
                lastRound = BCPEvent.currentRound();
            }

        } else if (hw->displayState != DISPLAY_EVENT_COUNTDOWN) {
            // timer active, the countdown is redrawn by the Hw implementation
            MAIN_DEBUG("Displaying countdown.");
            hw->displayCountdown(BCPEvent);
        }

    } else if (!BCPEvent.invalid()) {
        // event ID not verified yet (no response from BCP), keep the splash screen
        if (hw->displayState != DISPLAY_BOOT) {
            MAIN_DEBUG("Event ID not verified yet, displaying splash screen.");
            hw->splashScreen();
        }

    } else if (hw->displayState != DISPLAY_CONFIG_REQUIRED) {
        // invalid event ID, run the config server with no timeout and ask for configuration
        MAIN_DEBUG("Invalid event ID, starting config server.");
        if (Config.isConfigServerRunning()) {
            Config.stopConfigServer();
        }
        Config.startConfigServer(0);
        hw->configServerMsg("Invalid event ID");
        wasInvalidEvent = true;
    }
}

// ---- core Arduino functions ----

void setup() {
#ifdef BCP_DEV
    debugOut_ = &Serial;
#endif
    hw = &Tc001::getInstance(debugOut_);
    hw->splashScreen();
    if (!hw->ensureConnection()) {
        MAIN_DEBUG("Connection not established, rebooting in 5 seconds...");
        delay(5000);
        hw->reboot();
    }
    Config.debugOut = debugOut_;
    // Config.erase(); // for testing only - remove in production
    BCPEvent.setID(Config.eventId());
    if (BCPEvent.valid()) {
        MAIN_DEBUG("Event data restored from snapshot.");
    }
}

void loop() {
    static int failCount = 0;
    static time_t lastDataRefresh = 0;

    if (Config.configUpdated) {
        MAIN_DEBUG("Configuration updated.");
        Config.configUpdated = false;
        if (Config.eventId() != BCPEvent.fullId()) {
            MAIN_DEBUG("Event ID changed, updating BCP event.");
            hw->splashScreen();
            BCPEvent.setID(Config.eventId());
            lastDataRefresh = 0; // force data refresh
            failCount = 0;
        }
    }

    if (lastDataRefresh == 0 || millis() - lastDataRefresh >= DATA_REFRESH_INTERVAL_MS || hw->enforceUpdate) {
        MAIN_DEBUG("Refreshing BCP event data...");
        hw->enforceUpdate = false;
        lastDataRefresh = millis();
        if (BCPEvent.refreshData()) {
            MAIN_DEBUG("BCP event data refreshed successfully");
            failCount = 0;
        } else if (WiFi.status() != WL_CONNECTED) {
            // WiFi outage: keep displaying the last data, rebooting would not help
            MAIN_DEBUG("Failed to refresh BCP event data, WiFi disconnected, reconnecting");
            WiFi.reconnect();
        } else if (BCPEvent.lastStatus() < 0) {
            // connection error while WiFi is up, may be stuck TLS/HTTP
            failCount++;
            MAIN_DEBUG("Failed to refresh BCP event data, connection error " + String(BCPEvent.lastStatus()) +
                       " (failure count " + String(failCount) + ")");
            if (failCount >= MAX_CONNECTION_FAILURES) {
                MAIN_DEBUG("Too many consecutive connection failures, rebooting device");
                hw->reboot();
            }
        } else if (BCPEvent.lastStatus() > 0) {
            // server responded (e.g. 5xx), the connection works
            failCount = 0;
            MAIN_DEBUG("Failed to refresh BCP event data, HTTP status " + String(BCPEvent.lastStatus()));
        }
        displayUpdate();

    } else if (hw->redrawRequested) {
        displayUpdate();
    }

#ifdef BCP_DEV
    devConsoleTick(hw);
#endif
    hw->tick();
    delay(10);
}