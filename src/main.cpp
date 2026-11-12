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

#define DISPLAY_STATE (hw->displayState == DISPLAY_EVENT_COUNTDOWN ? "COUNTDOWN" : \
                       hw->displayState == DISPLAY_EVENT_NAME ? "EVENT_NAME" : \
                       hw->displayState == DISPLAY_EVENT_ROUND ? "EVENT_ROUND" : \
                       hw->displayState == DISPLAY_BOOT ? "BOOT" : \
                       hw->displayState == DISPLAY_DONT_UPDATE ? "DONT_UPDATE" : \
                       hw->displayState == DISPLAY_ENFORCE_REFRESH ? "ENFORCE_REFRESH" : \
                       "UNKNOWN")

// ---- update display after data refresh ----
void displayUpdate() {
    MAIN_DEBUG("Updating display, current state: " + String(DISPLAY_STATE));

    if (hw->displayState == DISPLAY_DONT_UPDATE) {
        return;
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
                hw->displayState = DISPLAY_EVENT_NAME;
            }

        } else if (BCPEvent.timerLength() <= 0) {
            // event running but no timer active, show round only
            static int lastRound = -1;
            if (hw->displayState != DISPLAY_EVENT_ROUND || lastRound != BCPEvent.currentRound()) {
                MAIN_DEBUG("Displaying event round.");
                hw->displayEventRound(BCPEvent);
                lastRound = BCPEvent.currentRound();
                hw->displayState = DISPLAY_EVENT_ROUND;
            }

        } else if (hw->displayState != DISPLAY_EVENT_COUNTDOWN) {
            // timer active, just update state, rest will be handled in loop()
            hw->displayState = DISPLAY_EVENT_COUNTDOWN;

        }

    } else if (!BCPEvent.invalid()) {
        // event ID not verified yet (no response from BCP), keep the splash screen
        if (hw->displayState != DISPLAY_BOOT) {
            MAIN_DEBUG("Event ID not verified yet, displaying splash screen.");
            hw->splashScreen();
            hw->displayState = DISPLAY_BOOT;
        }

    } else if (!Config.isConfigServerRunning() || hw->displayState == DISPLAY_ENFORCE_REFRESH) {
        // invalid event ID, start config mode if not already running
        hw->displayState = DISPLAY_BOOT;
        if (Config.isConfigServerRunning()) {
            // stop existing server first and make sure it is running with no timeout
            Config.stopConfigServer();
        }
        Config.startConfigServer(0);
        hw->configServerMsg("Invalid event ID, please configure.");
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

    bool configUpdated = false;
    if (Config.configUpdated) {
        MAIN_DEBUG("Configuration updated.");
        Config.configUpdated = false;
        if (Config.eventId() != BCPEvent.fullId()) {
            MAIN_DEBUG("Event ID changed, updating BCP event.");
            hw->displayState = DISPLAY_DONT_UPDATE;
            hw->splashScreen();
            BCPEvent.setID(Config.eventId());
            lastDataRefresh = 0; // force data refresh
            failCount = 0;
            configUpdated = true;
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
        if (configUpdated) {
            hw->displayState = DISPLAY_ENFORCE_REFRESH;
            configUpdated = false;
        }
        displayUpdate();

    } else if (hw->displayState == DISPLAY_ENFORCE_REFRESH) {
        displayUpdate();
    }

#ifdef BCP_DEV
    devConsoleTick(hw);
#endif
    hw->tick();
    delay(10);
}