// Countdown text, colour and round indicator layout, independent of the display hardware
// (c) 2025 Honza Skýpala
// WTFPL license applies

#pragma once

#include <Arduino.h>
#include <time.h>

// ---- Countdown ----

enum CountdownColor {
    COUNTDOWN_WHITE,    // round running
    COUNTDOWN_YELLOW,   // below yellow threshold
    COUNTDOWN_RED,      // below red threshold (incl. over time)
    COUNTDOWN_GREEN,    // round not started yet, time to start
    COUNTDOWN_MAGENTA,  // paused, blink on
    COUNTDOWN_BLACK     // paused, blink off
};

struct CountdownView {
    char text[12];         // "mm:ss" / "h:mm:ss" / "hh:mm:ss", "-" prefix over time; '/' = X glyph for overflow
    CountdownColor color;
    bool hmmss;            // true: h:mm:ss font, false: mm:ss font
    int x;                 // cursor x position
};

// remaining = round end epoch - now; blinkOn = phase of the paused blinking;
// startPhase: remaining above timerLength is the time to the round start (green, BCP);
// ready: countdown not started yet, white without thresholds
CountdownView formatCountdown(long remaining, uint32_t timerLength, bool paused, long pausedRemaining,
                              long yellowThreshold, long redThreshold, bool blinkOn,
                              bool startPhase = true, bool ready = false);

// ---- Countdown as the display draws it (BCP round, offline countdown, chess clock players) ----

enum TimerPhase : uint8_t {
    TIMER_RUNNING,  // remaining = endEpoch - now
    TIMER_PAUSED,   // remaining fixed, blinks magenta
    TIMER_READY,    // not started (reset), remaining fixed, white
    TIMER_WAITING   // stopped, remaining fixed, colours by the thresholds (chess player not on turn)
};

struct CountdownTimer {
    TimerPhase phase = TIMER_READY;
    time_t endEpoch = 0;     // TIMER_RUNNING
    long remaining = 0;      // not TIMER_RUNNING: seconds shown
    uint32_t length = 0;     // round length; up to one hour mm:ss, longer h:mm:ss
    bool startPhase = false; // remaining above length = time to the round start (BCP)

    long remainingAt(time_t now) const { return phase == TIMER_RUNNING ? (long) (endEpoch - now) : remaining; }
};

constexpr int MAX_COUNTDOWN_TIMERS = 2;

// What a mode shows: one countdown, or one per player (chess clock). A display that has room for only
// one shows the active timer (the first one if none is active).
struct CountdownDisplay {
    CountdownTimer timers[MAX_COUNTDOWN_TIMERS];
    int count = 1;
    int active = -1;          // index of the running player (chess clock), -1 = none
    long yellowThreshold = 0; // seconds
    long redThreshold = 0;
    int currentRound = 0;     // round indicator, numberOfRounds 0 = none
    int numberOfRounds = 0;
};

CountdownView formatCountdown(const CountdownTimer& timer, time_t now, long yellowThreshold, long redThreshold,
                              bool blinkOn);

// ---- Acoustic signals at thresholds ----

enum BeepSignal {
    BEEP_NONE,
    BEEP_YELLOW,    // yellow threshold reached: one long beep
    BEEP_RED        // red threshold reached: three long beeps
};

// prevRemaining / remaining = seconds left at the previous / current redraw (paused: paused time remaining);
// red wins when both thresholds are crossed at once
BeepSignal detectThresholdCrossing(long prevRemaining, long remaining, long yellowThreshold, long redThreshold);

// true while the buzzer is on at elapsedMs since the start of the signal, false after the pattern ends
bool isBeepOn(BeepSignal signal, unsigned long elapsedMs);

// length of the signal pattern in milliseconds
unsigned long beepPatternLength(BeepSignal signal);

// ---- Round indicator (bottom row) ----

enum RoundPixel : uint8_t {
    ROUND_PIXEL_OFF,
    ROUND_PIXEL_INACTIVE,
    ROUND_PIXEL_ACTIVE
};

// Fills out[0..width-1]; all off when totalRounds <= 0
void layoutRoundIndicator(int currentRound, int totalRounds, int width, RoundPixel out[]);
