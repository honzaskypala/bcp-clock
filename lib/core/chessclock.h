// Chess clock: two button controlled countdowns, one runs at a time, independent of the hardware
// (c) 2025 Honza Skýpala
// WTFPL license applies

#pragma once

#include <stdint.h>
#include <time.h>
#include <buttons.h>
#include <countdown.h>
#include <manualtimer.h>

// Player 0 sits at the left, player 1 at the right. As on a chess clock, a player presses the button on
// their side to end their turn: Right starts player 0 (bar at the left), Left starts player 1 (bar at the right).
// Plain data, saved to NVS as it is.
struct ChessClock {
    ManualTimer players[2];
    int8_t active = -1;        // player on turn (running or paused), -1 = reset

    void reset(long playerLength);
    bool isReset() const { return active < 0; }
};

// Right / Left: start, resume or switch to that player (nothing if already running); short Mid: pause the
// running player, reset a paused clock, switch the mode when reset
TimerAction handleChessButton(ChessClock& clock, ButtonEvent event, long playerLength, time_t now);

// Both players, the active one marked; the thresholds are halved (they are set for the whole round)
CountdownDisplay chessCountdown(const ChessClock& clock, long playerLength, long yellowThreshold, long redThreshold);
