// Round countdown clock firmware for Ulanzi TC001 desktop clock hardware,
// fetching the data from www.bestcoastpairings.com
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <config.h>
#include <bcpevent.h>
#include <hw.h>
#include <tc001.h>
#include <policy.h>
#ifdef BCP_DEV
#include "devconsole.h"
#endif

#define DATA_REFRESH_INTERVAL_MS 60000 // 60 seconds
#define MAX_CONNECTION_FAILURES 5      // reboot after this many consecutive connection errors (stuck TLS/HTTP)

Print *debugOut_ = nullptr;  // Optional debug output
#define MAIN_DEBUG(msg) if (debugOut_) { debugOut_->print("[Main] "); debugOut_->println(msg); }

Hw *hw;
String mdnsHostname;  // hostname announced over mDNS

// ---- mDNS: the config page is also reachable at http://<hostname>.local ----
void startMdns() {
    mdnsHostname = Config.hostname();
    if (MDNS.begin(mdnsHostname.c_str())) {
        MAIN_DEBUG("mDNS started: http://" + mdnsHostname + ".local");
    } else {
        MAIN_DEBUG("mDNS start failed");
    }
}

// ---- update display after data refresh ----
// The display state is set by the Hw implementation; here it is only read to avoid redrawing
// what is already shown.
void updateDisplay() {
    static DisplayMemory memory;
    DisplayInputs in;
    in.state = hw->displayState;
    in.redrawRequested = hw->redrawRequested;
    in.valid = BCPEvent.isValid();
    in.invalid = BCPEvent.isInvalid();
    in.started = BCPEvent.isStarted();
    in.ended = BCPEvent.isEnded();
    in.timerLength = BCPEvent.timerLength();
    in.name = BCPEvent.name();
    in.currentRound = BCPEvent.currentRound();
    hw->redrawRequested = false;
    MAIN_DEBUG("Updating display, current state: " + String(displayStateName(in.state)));

    DisplayDecision d = decideDisplay(in, memory);

    if (d.restartConfigServer && Config.isConfigServerRunning()) {
        // event ID became valid: restart config server with timeout
        Config.stopConfigServer();
        Config.startConfigServer();
    }

    switch (d.action) {
        case SHOW_EVENT_NAME:
            MAIN_DEBUG("Displaying event name.");
            hw->displayEventName(BCPEvent);
            break;
        case SHOW_EVENT_ROUND:
            MAIN_DEBUG("Displaying event round.");
            hw->displayEventRound(BCPEvent);
            break;
        case SHOW_COUNTDOWN:
            // the countdown is redrawn by the Hw implementation
            MAIN_DEBUG("Displaying countdown.");
            hw->displayCountdown(BCPEvent);
            break;
        case SHOW_SPLASH:
            MAIN_DEBUG("Event ID not verified yet, displaying splash screen.");
            hw->showSplashScreen();
            break;
        case SHOW_CONFIG_REQUIRED:
            // run the config server with no timeout and ask for configuration
            MAIN_DEBUG("Invalid event ID, starting config server.");
            if (Config.isConfigServerRunning()) {
                Config.stopConfigServer();
            }
            Config.startConfigServer(0);
            hw->showConfigServerMsg("Invalid event ID");
            break;
        case SHOW_NOTHING_NEW:
            break;
    }
}

// ---- core Arduino functions ----

void setup() {
#ifdef BCP_DEV
    debugOut_ = &Serial;
#endif
    hw = &Tc001::getInstance(debugOut_);
    hw->showSplashScreen();
    WiFi.setHostname(Config.hostname().c_str());   // must be set before WiFi starts
    if (!hw->ensureConnection()) {
        MAIN_DEBUG("Connection not established, rebooting in 5 seconds...");
        delay(5000);
        hw->reboot();
    }
    Config.debugOut = debugOut_;
    startMdns();
    // Config.erase(); // for testing only - remove in production
    BCPEvent.setID(Config.eventId());
    if (BCPEvent.isValid()) {
        MAIN_DEBUG("Event data restored from snapshot.");
    }
}

void loop() {
    static int failCount = 0;
    static unsigned long lastDataRefresh = 0;   // millis() of the last refresh, 0 = refresh now

    if (Config.configUpdated) {
        MAIN_DEBUG("Configuration updated.");
        Config.configUpdated = false;
        if (Config.hostname() != mdnsHostname) {
            MAIN_DEBUG("Hostname changed, restarting mDNS.");
            WiFi.setHostname(Config.hostname().c_str());   // DHCP hostname applies at the next WiFi connection
            MDNS.end();
            startMdns();
        }
        if (Config.eventId() != BCPEvent.fullId()) {
            MAIN_DEBUG("Event ID changed, updating BCP event.");
            hw->showSplashScreen();
            BCPEvent.setID(Config.eventId());
            lastDataRefresh = 0; // force data refresh
            failCount = 0;
        }
    }

    if (lastDataRefresh == 0 || millis() - lastDataRefresh >= DATA_REFRESH_INTERVAL_MS || hw->enforceUpdate) {
        MAIN_DEBUG("Refreshing BCP event data...");
        hw->enforceUpdate = false;
        lastDataRefresh = millis();
        bool ok = BCPEvent.refreshData();
        RefreshDecision r = decideRefresh(ok, WiFi.status() == WL_CONNECTED, BCPEvent.lastStatus(),
                                            failCount, MAX_CONNECTION_FAILURES);
        failCount = r.failCount;
        switch (r.outcome) {
            case REFRESH_SUCCESS:
                MAIN_DEBUG("BCP event data refreshed successfully");
                break;
            case REFRESH_WIFI_DOWN:
                MAIN_DEBUG("Failed to refresh BCP event data, WiFi disconnected, reconnecting");
                WiFi.reconnect();
                break;
            case REFRESH_CONNECTION_ERROR:
            case REFRESH_REBOOT:
                MAIN_DEBUG("Failed to refresh BCP event data, connection error " + String(BCPEvent.lastStatus()) +
                           " (failure count " + String(failCount) + ")");
                if (r.outcome == REFRESH_REBOOT) {
                    MAIN_DEBUG("Too many consecutive connection failures, rebooting device");
                    hw->reboot();
                }
                break;
            case REFRESH_HTTP_ERROR:
                MAIN_DEBUG("Failed to refresh BCP event data, HTTP status " + String(BCPEvent.lastStatus()));
                break;
            case REFRESH_NO_REQUEST:
                break;
        }
        updateDisplay();

    } else if (hw->redrawRequested) {
        updateDisplay();
    }

#ifdef BCP_DEV
    tickDevConsole(hw);
#endif
    hw->tick();
    delay(10);
}