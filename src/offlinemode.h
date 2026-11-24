// Offline mode: countdown of a length set on the config page, controlled by the buttons
// (c) 2025 Honza Skýpala
// WTFPL license applies

#pragma once

#include <buildfeatures.h>

#if BCP_MODE_OFFLINE

#include <manualtimer.h>
#include "mode.h"

class OfflineMode : public Mode {
public:
    ModeId id() const override { return MODE_OFFLINE; }
    const char *name() const override { return "Offline"; }
    void enter(Hw& hw) override;
    void tick(Hw& hw) override;
    ModeAction onButton(Hw& hw, ButtonEvent event) override;
    void onConfigUpdated(Hw& hw) override;

    const ManualTimer& timer() const { return timer_; }  // dev console

private:
    bool loaded_ = false;
    ManualTimer timer_;       // saved in NVS at every change, runs on while the clock is off

    CountdownDisplay countdown() const;
    void showChange(Hw& hw);  // redraw after a change of the timer
};

extern OfflineMode offlineMode;

#endif
