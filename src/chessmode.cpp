// Chess clock mode: half of the round length set on the config page for each of two players
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include "chessmode.h"

#if BCP_MODE_CHESS

#include <config.h>
#include <devlog.h>

#define CHESS_DEBUG(msg) DEV_LOG("Chess", msg)

static constexpr const char *STATE_KEY = "chess";

ChessMode chessMode;

static long playerLength() {
    return Config.chessLength() / 2;
}

void ChessMode::enter(Hw& hw) {
    if (!loaded_) {
        // the clock of the last game, also if it kept running while the clock was off
        if (!Config.loadState(STATE_KEY, &clock_, sizeof(clock_))) {
            clock_.reset(playerLength());
        }
        loaded_ = true;
    }
}

void ChessMode::tick(Hw& hw) {
    if (hw.redrawRequested) {
        hw.redrawRequested = false;
        hw.displayCountdown(countdown());
    }
}

ModeAction ChessMode::onButton(Hw& hw, ButtonEvent event) {
    switch (handleChessButton(clock_, event, playerLength(), time(nullptr))) {
        case TIMER_CHANGED:
            CHESS_DEBUG("Active player " + String(clock_.active) + ", remaining " +
                        String(clock_.players[0].remainingAt(time(nullptr))) + " / " +
                        String(clock_.players[1].remainingAt(time(nullptr))) + " s");
            Config.saveState(STATE_KEY, &clock_, sizeof(clock_));
            showChange(hw);
            return MODE_STAY;
        case TIMER_NEXT_MODE:
            return MODE_NEXT;
        default:
            return MODE_STAY;
    }
}

void ChessMode::onConfigUpdated(Hw& hw) {
    if (clock_.isReset() && clock_.players[0].remaining != playerLength()) {
        clock_.reset(playerLength());  // a game in progress keeps its times
        Config.saveState(STATE_KEY, &clock_, sizeof(clock_));
    }
    showChange(hw);  // also the thresholds
}

void ChessMode::showChange(Hw& hw) {
    if (hw.displayState == DISPLAY_EVENT_COUNTDOWN) {
        hw.updateCountdown(countdown());
    } else if (!isMessageShown(hw.displayState)) {
        hw.redrawRequested = true;  // nothing drawn yet; after a message the Hw requests the redraw itself
    }
}

CountdownDisplay ChessMode::countdown() const {
    return chessCountdown(clock_, playerLength(), Config.yellowThreshold(), Config.redThreshold());
}

#endif
