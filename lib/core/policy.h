// Decisions of the main loop and the display, independent of the hardware
// (c) 2025 Honza Skýpala
// WTFPL license applies

#pragma once

#include <Arduino.h>
#include <displaystate.h>

// ---- What to show after a data refresh ----

enum DisplayAction {
    SHOW_NOTHING_NEW,      // keep what is shown
    SHOW_EVENT_NAME,
    SHOW_EVENT_ROUND,
    SHOW_COUNTDOWN,
    SHOW_SPLASH,           // event ID not verified yet
    SHOW_CONFIG_REQUIRED   // invalid event ID: config server with no timeout + message
};

struct DisplayInputs {
    DisplayState state;
    bool redrawRequested;
    bool valid, invalid;       // event ID state
    bool started, ended;
    uint32_t timerLength;
    String name;
    int currentRound;
};

// What was shown, kept between calls
struct DisplayMemory {
    bool wasInvalidEvent = false;
    String eventName;
    int lastRound = -1;
};

struct DisplayDecision {
    DisplayAction action;
    bool restartConfigServer;  // event ID became valid: restart a running config server with timeout
};

DisplayDecision decideDisplay(const DisplayInputs& in, DisplayMemory& memory);

// ---- Reaction to a data refresh result ----

enum RefreshOutcome {
    REFRESH_SUCCESS,           // data or 404 received
    REFRESH_WIFI_DOWN,         // reconnect WiFi, keep the last data
    REFRESH_CONNECTION_ERROR,  // counted toward reboot
    REFRESH_REBOOT,            // too many consecutive connection errors
    REFRESH_HTTP_ERROR,        // server responded with an error, the connection works
    REFRESH_NO_REQUEST         // no request was made (empty event ID)
};

struct RefreshDecision {
    RefreshOutcome outcome;
    int failCount;
};

RefreshDecision decideRefresh(bool ok, bool wifiConnected, int lastStatus, int failCount, int maxFailures);

// ---- User messages printed to the display (Tc001 is a Print) ----

enum MessageKind {
    MESSAGE_ERROR,       // "Error: ..." red scrolling text
    MESSAGE_CONNECTING,  // "Connecting ..." progress indicator
    MESSAGE_CONNECTED,   // "Connected ..." ignored
    MESSAGE_OTHER        // white scrolling text
};

MessageKind classifyMessage(const char *line);
