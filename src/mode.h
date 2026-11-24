// Operating mode of the clock: owns the display content and the buttons while it is the current mode
// (c) 2025 Honza Skýpala
// WTFPL license applies

#pragma once

#include <hw.h>
#include <modeid.h>

enum ModeAction {
    MODE_STAY,
    MODE_NEXT   // switch to the next mode (short Mid press)
};

// The framework (main.cpp) handles the long Mid press (config page) and the mode switching; a mode
// draws its content when hw.redrawRequested is set (after the mode name or a message finished).
class Mode {
public:
    virtual ModeId id() const = 0;
    virtual const char *name() const = 0;     // shown after switching, font f3x5
    virtual bool needsNetwork() const { return false; }  // true: WiFi must be connected before enter()
    virtual void enter(Hw& hw) = 0;           // became the current mode (also at boot)
    virtual void leave(Hw& hw) {}
    virtual void tick(Hw& hw) = 0;            // every loop()
    virtual ModeAction onButton(Hw& hw, ButtonEvent event) = 0;  // Left, Right, short Mid
    virtual void onConfigUpdated(Hw& hw) {}   // saved on the config page
};
