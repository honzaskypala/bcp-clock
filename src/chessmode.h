// Chess clock mode: half of the round length set on the config page for each of two players
// (c) 2025 Honza Skýpala
// WTFPL license applies

#pragma once

#include <buildfeatures.h>

#if BCP_MODE_CHESS

#include <chessclock.h>
#include "mode.h"

class ChessMode : public Mode {
public:
    ModeId id() const override { return MODE_CHESS; }
    const char *name() const override { return "Chess clock"; }
    void enter(Hw& hw) override;
    void tick(Hw& hw) override;
    ModeAction onButton(Hw& hw, ButtonEvent event) override;
    void onConfigUpdated(Hw& hw) override;

    const ChessClock& clock() const { return clock_; }  // dev console

private:
    bool loaded_ = false;
    ChessClock clock_;        // saved in NVS at every change, runs on while the clock is off

    CountdownDisplay countdown() const;
    void showChange(Hw& hw);  // redraw after a change of the clock
};

extern ChessMode chessMode;

#endif
