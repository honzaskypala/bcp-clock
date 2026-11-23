// Countdown text, colour and round indicator layout, independent of the display hardware
// (c) 2025 Honza Skýpala
// WTFPL license applies

#pragma once

#include <Arduino.h>

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

// remaining = round end epoch - now; blinkOn = phase of the paused blinking
CountdownView formatCountdown(long remaining, uint32_t timerLength, bool paused, uint32_t pausedRemaining,
                              long yellowThreshold, long redThreshold, bool blinkOn);

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
