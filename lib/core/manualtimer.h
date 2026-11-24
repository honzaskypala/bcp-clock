// Countdown controlled by buttons (offline mode, a chess clock player), independent of the hardware
// (c) 2025 Honza Skýpala
// WTFPL license applies

#pragma once

#include <stdint.h>
#include <time.h>
#include <buttons.h>
#include <countdown.h>

enum ManualTimerState : uint8_t {
    MANUAL_READY,    // reset to the configured length
    MANUAL_RUNNING,
    MANUAL_PAUSED
};

// Like the BCP countdown, a running timer is computed from its end time, so it keeps running while the
// clock is off or in another mode. Plain data, saved to NVS as it is.
struct ManualTimer {
    uint8_t state = MANUAL_READY;  // ManualTimerState
    int64_t endEpoch = 0;          // MANUAL_RUNNING: remaining = endEpoch - now
    int32_t remaining = 0;         // MANUAL_READY / MANUAL_PAUSED

    bool isRunning() const { return state == MANUAL_RUNNING; }
    long remainingAt(time_t now) const { return isRunning() ? (long) (endEpoch - now) : remaining; }

    void reset(long length);
    bool start(time_t now);   // from ready or paused; false if already running
    bool pause(time_t now);   // false if not running

    // waiting: a paused timer is shown without blinking (the chess player not on turn)
    CountdownTimer timer(uint32_t length, bool waiting = false) const;
};

enum TimerAction {
    TIMER_UNCHANGED,
    TIMER_CHANGED,    // save and redraw
    TIMER_NEXT_MODE   // short Mid press while stopped: switch the mode
};

// Offline mode: Right starts / resumes, short Mid pauses a running countdown or switches the mode,
// Left resets a stopped countdown to length
TimerAction handleOfflineButton(ManualTimer& timer, ButtonEvent event, long length, time_t now);
