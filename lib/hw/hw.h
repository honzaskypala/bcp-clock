// Abstract interface for hardware specific implementation
// You need to implement this interface for your hardware platform.
// (c) 2025 Honza Skýpala
// WTFPL license applies

#pragma once

#include "bcpevent.h"
#include <displaystate.h>

class Hw {
public:
    virtual void splashScreen(bool showProgress = true) {};
    virtual bool ensureConnection() = 0;

    static volatile DisplayState displayState;
    static volatile bool enforceUpdate;    // refresh data now (right button, dev console)
    static volatile bool redrawRequested;  // redraw from current data without fetching (config message finished)

    virtual void displayEventName(const CBCPEvent& event) = 0;
    virtual void displayEventRound(const CBCPEvent& event) = 0;
    virtual void displayCountdown(const CBCPEvent& event) = 0;  // the event must outlive the countdown

    virtual void tick() {};

    virtual void configServerMsg(const char *msg) = 0;

    virtual void reboot() = 0;
};
