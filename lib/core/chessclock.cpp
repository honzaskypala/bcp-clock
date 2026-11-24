// Chess clock: two button controlled countdowns, one runs at a time, independent of the hardware
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include "chessclock.h"

void ChessClock::reset(long playerLength) {
    players[0].reset(playerLength);
    players[1].reset(playerLength);
    active = -1;
}

static TimerAction startPlayer(ChessClock& clock, int player, time_t now) {
    if (clock.active == player && clock.players[player].isRunning()) return TIMER_UNCHANGED;
    if (clock.active >= 0) {
        clock.players[clock.active].pause(now);  // no-op when paused
    }
    clock.players[player].start(now);
    clock.active = player;
    return TIMER_CHANGED;
}

TimerAction handleChessButton(ChessClock& clock, ButtonEvent event, long playerLength, time_t now) {
    switch (event) {
        case BUTTON_RIGHT:
            return startPlayer(clock, 0, now);
        case BUTTON_LEFT:
            return startPlayer(clock, 1, now);
        case BUTTON_MID_SHORT:
            if (clock.isReset()) return TIMER_NEXT_MODE;
            if (!clock.players[clock.active].pause(now)) {
                clock.reset(playerLength);  // paused: back to the configured time, the bar disappears
            }
            return TIMER_CHANGED;
        default:
            return TIMER_UNCHANGED;
    }
}

CountdownDisplay chessCountdown(const ChessClock& clock, long playerLength, long yellowThreshold, long redThreshold) {
    CountdownDisplay c;
    c.count = 2;
    c.active = clock.active;
    for (int i = 0; i < 2; i++) {
        c.timers[i] = clock.players[i].timer(playerLength, i != clock.active);
    }
    c.yellowThreshold = yellowThreshold / 2;
    c.redThreshold = redThreshold / 2;
    return c;
}
