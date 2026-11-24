// Offline mode: countdown of a length set on the config page, controlled by the buttons
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include "offlinemode.h"

#if BCP_MODE_OFFLINE

#include <config.h>
#include <devlog.h>

#define OFFLINE_DEBUG(msg) DEV_LOG("Offline", msg)

static constexpr const char *STATE_KEY = "offline";

OfflineMode offlineMode;

void OfflineMode::enter(Hw& hw) {
    if (!loaded_) {
        // the countdown of the last run, also if it kept running while the clock was off
        if (!Config.loadState(STATE_KEY, &timer_, sizeof(timer_))) {
            timer_.reset(Config.offlineLength());
        }
        loaded_ = true;
    }
}

void OfflineMode::tick(Hw& hw) {
    if (hw.redrawRequested) {
        hw.redrawRequested = false;
        hw.displayCountdown(countdown());
    }
}

ModeAction OfflineMode::onButton(Hw& hw, ButtonEvent event) {
    switch (handleOfflineButton(timer_, event, Config.offlineLength(), time(nullptr))) {
        case TIMER_CHANGED:
            OFFLINE_DEBUG("Countdown state " + String(timer_.state) + ", remaining " +
                          String(timer_.remainingAt(time(nullptr))) + " s");
            Config.saveState(STATE_KEY, &timer_, sizeof(timer_));
            showChange(hw);
            return MODE_STAY;
        case TIMER_NEXT_MODE:
            return MODE_NEXT;
        default:
            return MODE_STAY;
    }
}

void OfflineMode::onConfigUpdated(Hw& hw) {
    if (timer_.state == MANUAL_READY && timer_.remaining != Config.offlineLength()) {
        timer_.reset(Config.offlineLength());  // a running or paused countdown keeps its length
        Config.saveState(STATE_KEY, &timer_, sizeof(timer_));
    }
    showChange(hw);  // also the thresholds
}

void OfflineMode::showChange(Hw& hw) {
    if (hw.displayState == DISPLAY_EVENT_COUNTDOWN) {
        hw.updateCountdown(countdown());
    } else if (!isMessageShown(hw.displayState)) {
        hw.redrawRequested = true;  // nothing drawn yet; after a message the Hw requests the redraw itself
    }
}

CountdownDisplay OfflineMode::countdown() const {
    CountdownDisplay c;
    c.timers[0] = timer_.timer(Config.offlineLength());
    c.yellowThreshold = Config.yellowThreshold();
    c.redThreshold = Config.redThreshold();
    return c;
}

#endif
