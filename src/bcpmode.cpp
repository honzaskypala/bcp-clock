// BCP mode: round countdown of a tournament on Best Coast Pairings
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include "bcpmode.h"

#if BCP_MODE_BCP

#include <Arduino.h>
#include <WiFi.h>
#include <limits.h>
#include <config.h>
#include <bcpevent.h>
#include <devlog.h>
#ifdef BCP_DEV
#include "devconsole.h"
#endif

#define DATA_REFRESH_INTERVAL_MS 60000 // 60 seconds
#if BCP_FEATURE_CONNECTION_MONITOR
#define MAX_CONNECTION_FAILURES 5      // reboot after this many consecutive WiFi outages or connection errors
#endif

#define BCP_DEBUG(msg) DEV_LOG("BCP", msg)

BcpMode bcpMode;

// ---- Mode ----

void BcpMode::enter(Hw& hw) {
    if (!idSet_ || Config.eventId() != BCPEvent.fullId()) {
        // at boot, or the event ID was changed on the config page in another mode
        BCPEvent.setID(Config.eventId());
        if (!idSet_ && BCPEvent.isValid()) {
            BCP_DEBUG("Event data restored from snapshot.");
        }
        idSet_ = true;
    }
    lastDataRefresh_ = 0;  // refresh now
    resetConnectionAlert(hw);
}

void BcpMode::leave(Hw& hw) {
#if BCP_FEATURE_CONNECTION_MONITOR
    hw.setConnectionAlert(CONNECTION_OK, false);  // the other modes do not monitor the connection
#endif
    if (hw.displayState == DISPLAY_CONFIG_REQUIRED && Config.isConfigServerRunning()) {
        // invalid event ID: the config server ran with no timeout, the other modes do not need it
        Config.stopConfigServer();
        Config.startConfigServer();
    }
}

void BcpMode::tick(Hw& hw) {
    if (hw.redrawRequested) {
        // a message finished (also ended by Right): show the content before the slow fetch
        updateDisplay(hw);
    }
    if (isRefreshDue(hw)) {
        refreshEventData(hw);
        updateDisplay(hw);
    }
}

ModeAction BcpMode::onButton(Hw& hw, ButtonEvent event) {
    switch (event) {
        case BUTTON_RIGHT:
            lastDataRefresh_ = 0;  // refresh now
            return MODE_STAY;
        case BUTTON_MID_SHORT:
            return MODE_NEXT;
        default:
            return MODE_STAY;
    }
}

// Applies a configuration saved on the config page
void BcpMode::onConfigUpdated(Hw& hw) {
    if (Config.eventId() != BCPEvent.fullId()) {
        BCP_DEBUG("Event ID changed, updating BCP event.");
        hw.showSplashScreen();
        BCPEvent.setID(Config.eventId());
        lastDataRefresh_ = 0; // force data refresh
        resetConnectionAlert(hw);
    } else if (hw.displayState == DISPLAY_EVENT_COUNTDOWN) {
        hw.updateCountdown(countdown());  // thresholds
    }
}

void BcpMode::resetConnectionAlert(Hw& hw) {
#if BCP_FEATURE_CONNECTION_MONITOR
    failCount_ = 0;
    connectionAlert_ = ConnectionAlert();
    hw.setConnectionAlert(connectionAlert_.problem, connectionAlert_.isIconDue());
#endif
}

// ---- Display ----

CountdownDisplay BcpMode::countdown() {
    CountdownDisplay c;
    CountdownTimer& t = c.timers[0];
    t.length = BCPEvent.timerLength();
    t.startPhase = true;  // remaining above the length = time to the round start
    if (BCPEvent.isTimerPaused()) {
        t.phase = TIMER_PAUSED;
        t.remaining = BCPEvent.pausedTimeRemaining();
    } else {
        t.phase = TIMER_RUNNING;
        t.endEpoch = BCPEvent.roundEndEpoch();
    }
    c.yellowThreshold = Config.yellowThreshold();
    c.redThreshold = Config.redThreshold();
    c.currentRound = BCPEvent.currentRound();
    c.numberOfRounds = BCPEvent.numberOfRounds();
    return c;
}

void BcpMode::applyDisplayAction(Hw& hw, DisplayAction action) {
    switch (action) {
        case SHOW_EVENT_NAME:
            BCP_DEBUG("Displaying event name.");
            hw.displayEventName(BCPEvent);
            break;
        case SHOW_EVENT_ROUND:
            BCP_DEBUG("Displaying event round.");
            hw.displayEventRound(BCPEvent);
            break;
        case SHOW_COUNTDOWN:
            // the countdown is redrawn by the Hw implementation
            BCP_DEBUG("Displaying countdown.");
            hw.displayCountdown(countdown());
            break;
        case SHOW_SPLASH:
            BCP_DEBUG("Event ID not verified yet, displaying splash screen.");
            hw.showSplashScreen();
            break;
        case SHOW_CONFIG_REQUIRED:
            // run the config server with no timeout and ask for configuration
            BCP_DEBUG("Invalid event ID, starting config server.");
            if (Config.isConfigServerRunning()) {
                Config.stopConfigServer();
            }
            Config.startConfigServer(0);
            hw.showConfigServerMsg("Invalid event ID");
            break;
        case SHOW_NOTHING_NEW:
            break;
    }
}

// The display state is set by the Hw implementation; here it is only read to avoid redrawing
// what is already shown.
void BcpMode::updateDisplay(Hw& hw) {
    DisplayInputs in;
    in.state = hw.displayState;
    in.redrawRequested = hw.redrawRequested;
    in.valid = BCPEvent.isValid();
    in.invalid = BCPEvent.isInvalid();
    in.started = BCPEvent.isStarted();
    in.ended = BCPEvent.isEnded();
    in.timerLength = BCPEvent.timerLength();
    in.name = BCPEvent.name();
    in.currentRound = BCPEvent.currentRound();
    hw.redrawRequested = false;
    BCP_DEBUG("Updating display, current state: " + String(displayStateName(in.state)));

    DisplayDecision d = decideDisplay(in, memory_);

    if (d.restartConfigServer && Config.isConfigServerRunning()) {
        // event ID became valid: restart config server with timeout
        Config.stopConfigServer();
        Config.startConfigServer();
    }
    applyDisplayAction(hw, d.action);
    if (d.action == SHOW_NOTHING_NEW && in.state == DISPLAY_EVENT_COUNTDOWN) {
        hw.updateCountdown(countdown());  // refreshed timer data (pause, time changes)
    }
}

// ---- Data refresh ----

bool BcpMode::isRefreshDue(Hw& hw) {
    return lastDataRefresh_ == 0 || millis() - lastDataRefresh_ >= DATA_REFRESH_INTERVAL_MS || hw.enforceUpdate;
}

// Fetches the event data and reacts to failures (WiFi reconnect; with the connection monitor also the
// connection alert and a reboot after repeated WiFi outages or connection errors)
void BcpMode::refreshEventData(Hw& hw) {
    BCP_DEBUG("Refreshing BCP event data...");
    hw.enforceUpdate = false;
    lastDataRefresh_ = millis();
    bool ok = BCPEvent.refreshData();
    bool wifiConnected = WiFi.status() == WL_CONNECTED;
#ifdef BCP_DEV
    if (devWifiFailing) wifiConnected = false;
#endif
#if BCP_FEATURE_CONNECTION_MONITOR
    RefreshDecision r = decideRefresh(ok, wifiConnected, BCPEvent.lastStatus(), failCount_, MAX_CONNECTION_FAILURES);
    failCount_ = r.failCount;
    connectionAlert_ = nextConnectionAlert(connectionAlert_, r.outcome);
    hw.setConnectionAlert(connectionAlert_.problem, connectionAlert_.isIconDue());
#else
    RefreshDecision r = decideRefresh(ok, wifiConnected, BCPEvent.lastStatus(), 0, INT_MAX);  // never reboots
#endif
    switch (r.outcome) {
        case REFRESH_SUCCESS:
            BCP_DEBUG("BCP event data refreshed successfully");
            break;
        case REFRESH_WIFI_DOWN:
#if BCP_FEATURE_CONNECTION_MONITOR
            BCP_DEBUG("Failed to refresh BCP event data, WiFi disconnected, reconnecting (failure count " +
                      String(failCount_) + ")");
#else
            BCP_DEBUG("Failed to refresh BCP event data, WiFi disconnected, reconnecting");
#endif
#ifdef BCP_DEV
            if (devWifiFailing) break;
#endif
            WiFi.reconnect();
            break;
        case REFRESH_CONNECTION_ERROR:
#if BCP_FEATURE_CONNECTION_MONITOR
            BCP_DEBUG("Failed to refresh BCP event data, connection error " + String(BCPEvent.lastStatus()) +
                      " (failure count " + String(failCount_) + ")");
#else
            BCP_DEBUG("Failed to refresh BCP event data, connection error " + String(BCPEvent.lastStatus()));
#endif
            break;
        case REFRESH_REBOOT:
            BCP_DEBUG("Too many consecutive connection failures, rebooting device");
            hw.reboot();
            break;
        case REFRESH_HTTP_ERROR:
            BCP_DEBUG("Failed to refresh BCP event data, HTTP status " + String(BCPEvent.lastStatus()));
            break;
        case REFRESH_NO_REQUEST:
            break;
    }
}

#if defined(BCP_DEV) && BCP_FEATURE_CONNECTION_MONITOR
void BcpMode::printConnectionStatus(Print& out) {
    static const char *problemNames[] = { "OK", "WIFI", "BCP" };
    out.printf("connection: %s, failures %d (icon %s), reboot count %d/%d, wififail=%d\n",
               problemNames[connectionAlert_.problem], connectionAlert_.failures,
               connectionAlert_.isIconDue() ? "on" : "off", failCount_, MAX_CONNECTION_FAILURES, devWifiFailing);
}
#endif

#endif
