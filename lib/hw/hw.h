// Abstract interface for hardware specific implementation
// You need to implement this interface for your hardware platform.
// (c) 2025 Honza Skýpala
// WTFPL license applies

#pragma once

#include <buildfeatures.h>
#if BCP_MODE_BCP
#include "bcpevent.h"
#endif
#include <buttons.h>
#include <countdown.h>
#include <displaystate.h>
#include <statusalert.h>

class Hw {
public:
    virtual void showSplashScreen(bool showProgress = true) {};
    // Connects to WiFi (blocking, shows the progress). required: the captive portal runs when no network
    // connects (5 min, then false); otherwise only stored and open networks are tried. abortable: a short Mid
    // press leaves the portal (false is returned). Without a connection the time is taken from the RTC.
    virtual bool ensureConnection(bool required, bool abortable) = 0;
    virtual bool isConnected() = 0;

    static volatile DisplayState displayState;
    static volatile bool enforceUpdate;    // refresh BCP data now (dev console)
    static volatile bool redrawRequested;  // the current mode redraws its content (a message finished)

    // ---- Buttons ----
    // Next button press, BUTTON_NONE if none. A press also ends the mode name or the config page address
    // (then redrawRequested is set).
    virtual ButtonEvent nextButtonEvent() = 0;

    // ---- Content of the modes ----
#if BCP_MODE_BCP
    virtual void displayEventName(const CBCPEvent& event) = 0;
    virtual void displayEventRound(const CBCPEvent& event) = 0;
#endif
    // A new countdown, copied and redrawn every second; the first redraw does not beep
    virtual void displayCountdown(const CountdownDisplay& countdown) = 0;
    // New data of the countdown shown (refresh, pause, player switch); threshold signals continue
    virtual void updateCountdown(const CountdownDisplay& countdown) = 0;

    // ---- Messages ----
    virtual void showModeName(const char *name) = 0;             // 5 s, or scrolled once if it does not fit
    virtual void showConfigServerMsg(const char *msg) = 0;       // message once, then the address loops
    virtual void showConfigServerAddress() = 0;                  // config page address scrolled once

    virtual void tick() {};

    // Connection problem after the last data refresh: blinking status indicator, icon if showIcon
    virtual void setConnectionAlert(ConnectionProblem problem, bool showIcon) {};

#if BCP_FEATURE_BRIGHTNESS
    // Display brightness in percent (10..100), applies at once
    virtual void setBrightness(uint8_t percent) {}
#endif

    virtual void reboot() = 0;
};
