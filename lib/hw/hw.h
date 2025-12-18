// Abstract interface for hardware specific implementation
// You need to implement this interface for your hardware platform.
// (c) 2025 Honza Skýpala
// WTFPL license applies

#pragma once

#include "bcpevent.h"

enum DisplayState {
    DISPLAY_BOOT,
    DISPLAY_EVENT_NAME,
    DISPLAY_EVENT_ROUND,
    DISPLAY_EVENT_COUNTDOWN,
    DISPLAY_DONT_UPDATE,
    DISPLAY_ENFORCE_REFRESH
};

class Hw {
public:
    virtual void splashScreen(bool showProgress = true) {};
    virtual bool ensureConnection() = 0;

    static volatile DisplayState displayState;
    static volatile bool enforceUpdate;

    virtual void displayEventName(const CBCPEvent& event) = 0;
    virtual void displayEventRound(const CBCPEvent& event) = 0;

    virtual void tick() {};

    virtual void configServerMsg(const char *msg) = 0;

    virtual void reboot() = 0;
};