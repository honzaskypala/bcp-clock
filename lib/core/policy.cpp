// Decisions of the main loop and the display, independent of the hardware
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include "policy.h"
#include <string.h>

// ---- What to show after a data refresh ----

DisplayDecision decideDisplay(const DisplayInputs& in, DisplayMemory& memory) {
    DisplayDecision d = { SHOW_NOTHING_NEW, false };

    if (in.state == DISPLAY_CONFIG_MESSAGE && !in.redrawRequested) {
        return d;  // config page address is scrolling, eventHandler requests a redraw when it finishes
    }

    if (in.valid) {
        if (memory.wasInvalidEvent) {
            d.restartConfigServer = true;
            memory.wasInvalidEvent = false;
        }

        bool isEventRunning = in.started && !in.ended;
        if (!isEventRunning) {
            // event not running, show event name
            if (in.state != DISPLAY_EVENT_NAME || memory.eventName != in.name) {
                d.action = SHOW_EVENT_NAME;
                memory.eventName = in.name;
            }

        } else if (in.timerLength == 0) {
            // event running but no timer active, show round only
            if (in.state != DISPLAY_EVENT_ROUND || memory.lastRound != in.currentRound) {
                d.action = SHOW_EVENT_ROUND;
                memory.lastRound = in.currentRound;
            }

        } else if (in.state != DISPLAY_EVENT_COUNTDOWN) {
            // timer active, the countdown is redrawn by the Hw implementation
            d.action = SHOW_COUNTDOWN;
        }

    } else if (!in.invalid) {
        // event ID not verified yet (no response from BCP), keep the splash screen
        if (in.state != DISPLAY_BOOT) {
            d.action = SHOW_SPLASH;
        }

    } else if (in.state != DISPLAY_CONFIG_REQUIRED) {
        // invalid event ID, run the config server with no timeout and ask for configuration
        d.action = SHOW_CONFIG_REQUIRED;
        memory.wasInvalidEvent = true;
    }
    return d;
}

// ---- Reaction to a data refresh result ----

RefreshDecision onRefreshResult(bool ok, bool wifiConnected, int lastStatus, int failCount, int maxFailures) {
    if (ok) {
        return { REFRESH_SUCCESS, 0 };
    } else if (!wifiConnected) {
        // WiFi outage: keep displaying the last data, rebooting would not help
        return { REFRESH_WIFI_DOWN, failCount };
    } else if (lastStatus < 0) {
        // connection error while WiFi is up, may be stuck TLS/HTTP
        failCount++;
        return { failCount >= maxFailures ? REFRESH_REBOOT : REFRESH_CONNECTION_ERROR, failCount };
    } else if (lastStatus > 0) {
        // server responded (e.g. 5xx), the connection works
        return { REFRESH_HTTP_ERROR, 0 };
    }
    return { REFRESH_NO_REQUEST, failCount };
}

// ---- User messages printed to the display ----

MessageKind classifyMessage(const char *line) {
    if (strncmp(line, "Error:", 6) == 0) return MESSAGE_ERROR;
    if (strncmp(line, "Connecting ", 11) == 0) return MESSAGE_CONNECTING;
    if (strncmp(line, "Connected ", 10) == 0) return MESSAGE_CONNECTED;
    return MESSAGE_OTHER;
}
