// What is shown on the display
// (c) 2025 Honza Skýpala
// WTFPL license applies

#pragma once

// What is shown on the display. Set only by the Hw implementation, read by main.
enum DisplayState {
    DISPLAY_BOOT,             // splash screen or connection messages
    DISPLAY_EVENT_NAME,       // event not running: scrolling event name
    DISPLAY_EVENT_ROUND,      // event running, no timer: "Round N"
    DISPLAY_EVENT_COUNTDOWN,  // round timer, redrawn every second
    DISPLAY_CONFIG_REQUIRED,  // invalid event ID: message + looping config page address
    DISPLAY_CONFIG_MESSAGE,   // config server started by the user: config page address scrolled once
    DISPLAY_MODE_NAME         // mode switched: its name for 5 s or scrolled once
};

// A message over the content of a mode: the mode does not draw until Hw::redrawRequested
inline bool isMessageShown(DisplayState state) {
    return state == DISPLAY_CONFIG_MESSAGE || state == DISPLAY_MODE_NAME;
}

inline const char *displayStateName(DisplayState state) {
    switch (state) {
        case DISPLAY_BOOT: return "BOOT";
        case DISPLAY_EVENT_NAME: return "EVENT_NAME";
        case DISPLAY_EVENT_ROUND: return "EVENT_ROUND";
        case DISPLAY_EVENT_COUNTDOWN: return "EVENT_COUNTDOWN";
        case DISPLAY_CONFIG_REQUIRED: return "CONFIG_REQUIRED";
        case DISPLAY_CONFIG_MESSAGE: return "CONFIG_MESSAGE";
        case DISPLAY_MODE_NAME: return "MODE_NAME";
    }
    return "UNKNOWN";
}
