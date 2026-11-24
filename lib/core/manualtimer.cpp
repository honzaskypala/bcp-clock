// Countdown controlled by buttons (offline mode, a chess clock player), independent of the hardware
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include "manualtimer.h"

// ---- ManualTimer ----

void ManualTimer::reset(long length) {
    state = MANUAL_READY;
    remaining = length;
    endEpoch = 0;
}

bool ManualTimer::start(time_t now) {
    if (isRunning()) return false;
    endEpoch = now + remaining;
    state = MANUAL_RUNNING;
    return true;
}

bool ManualTimer::pause(time_t now) {
    if (!isRunning()) return false;
    remaining = endEpoch - now;
    state = MANUAL_PAUSED;
    return true;
}

CountdownTimer ManualTimer::timer(uint32_t length, bool waiting) const {
    CountdownTimer t;
    t.length = length;
    t.endEpoch = endEpoch;
    t.remaining = remaining;
    switch (state) {
        case MANUAL_RUNNING: t.phase = TIMER_RUNNING; break;
        case MANUAL_PAUSED:  t.phase = waiting ? TIMER_WAITING : TIMER_PAUSED; break;
        default:             t.phase = TIMER_READY; break;
    }
    return t;
}

// ---- Offline mode ----

TimerAction handleOfflineButton(ManualTimer& timer, ButtonEvent event, long length, time_t now) {
    switch (event) {
        case BUTTON_RIGHT:
            return timer.start(now) ? TIMER_CHANGED : TIMER_UNCHANGED;
        case BUTTON_MID_SHORT:
            return timer.pause(now) ? TIMER_CHANGED : TIMER_NEXT_MODE;
        case BUTTON_LEFT:
            if (timer.isRunning()) return TIMER_UNCHANGED;
            if (timer.state == MANUAL_READY && timer.remaining == length) return TIMER_UNCHANGED;
            timer.reset(length);
            return TIMER_CHANGED;
        default:
            return TIMER_UNCHANGED;
    }
}
