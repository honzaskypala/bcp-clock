// Countdown text, colour and round indicator layout, independent of the display hardware
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include "countdown.h"
#include <stdio.h>
#include <string.h>

// ---- Countdown ----

// Fills in the time text and its position: mm:ss, or h:mm:ss / hh:mm:ss when hours are displayed;
// negative time keeps a single hour digit, "-hh:mm:ss" would not fit the display
static inline void formatTime(CountdownView& view, long remaining, bool dontDisplayHours, bool twoDigitHours) {
    const char *sign = remaining < 0 ? "-" : "";
    int absoluteTime = remaining < 0 ? -remaining : remaining;
    int hours = absoluteTime / 3600;
    int minutes = (absoluteTime % 3600) / 60;
    int seconds = absoluteTime % 60;

    if (dontDisplayHours) {
        // mm:ss
        view.hmmss = false;
        view.x = remaining < 0 ? 1 : 5;
        if (remaining < -3599) {
            strcpy(view.text, "-//://");    // To save space in the font, X bitmap mapped to / character
        } else {
            snprintf(view.text, sizeof(view.text), "%s%02d:%02d", sign, minutes, seconds);
        }
    } else {
        // h:mm:ss
        view.hmmss = true;
        view.x = remaining < 0 ? -1 : 3;
        if (remaining < -35999) {
            strcpy(view.text, "-/://://");  // To save space in the font, X bitmap mapped to / character
        } else if (twoDigitHours && remaining >= 0) {
            // hh:mm:ss
            view.x = 1;
            snprintf(view.text, sizeof(view.text), "%02d:%02d:%02d", hours, minutes, seconds);
        } else {
            snprintf(view.text, sizeof(view.text), "%s%d:%02d:%02d", sign, hours, minutes, seconds);
        }
    }
}

CountdownView formatCountdown(long remaining, uint32_t timerLength, bool paused, uint32_t pausedRemaining,
                              long yellowThreshold, long redThreshold, bool blinkOn) {
    CountdownView view;
    bool dontDisplayHours = timerLength <= 3600;

    if (paused) {
        // timer is paused
        remaining = pausedRemaining;
        view.color = blinkOn ? COUNTDOWN_MAGENTA : COUNTDOWN_BLACK;
    } else if (remaining <= redThreshold) {
        // we are below red threshold of the timer
        view.color = COUNTDOWN_RED;
    } else if (remaining <= yellowThreshold) {
        // we are below yellow threshold of the timer
        view.color = COUNTDOWN_YELLOW;
    } else if (remaining > timerLength || (remaining == timerLength && timerLength <= 3600)) {
        // event round not yet started, show time to start
        view.color = COUNTDOWN_GREEN;
        remaining -= timerLength;
        dontDisplayHours = remaining < 3600 && timerLength <= 3600;
    } else {
        view.color = COUNTDOWN_WHITE;
    }

    // a round that starts with two-digit hours keeps them until the end (09:59:59, ...)
    bool twoDigitHours = timerLength >= 36000 || remaining >= 36000;
    formatTime(view, remaining, dontDisplayHours, twoDigitHours);
    return view;
}

// ---- Acoustic signals at thresholds ----

static constexpr unsigned long BEEP_ON_MS = 2000;
static constexpr unsigned long BEEP_OFF_MS = 1000;

BeepSignal detectThresholdCrossing(long prevRemaining, long remaining, long yellowThreshold, long redThreshold) {
    if (prevRemaining > redThreshold && remaining <= redThreshold) return BEEP_RED;
    if (prevRemaining > yellowThreshold && remaining <= yellowThreshold) return BEEP_YELLOW;
    return BEEP_NONE;
}

static int beepCount(BeepSignal signal) {
    switch (signal) {
        case BEEP_YELLOW: return 1;
        case BEEP_RED:    return 3;
        default:          return 0;
    }
}

unsigned long beepPatternLength(BeepSignal signal) {
    int count = beepCount(signal);
    return count == 0 ? 0 : count * BEEP_ON_MS + (count - 1) * BEEP_OFF_MS;
}

bool isBeepOn(BeepSignal signal, unsigned long elapsedMs) {
    if (elapsedMs >= beepPatternLength(signal)) return false;
    return elapsedMs % (BEEP_ON_MS + BEEP_OFF_MS) < BEEP_ON_MS;
}

// ---- Round indicator (bottom row) ----

// Left offset of the round lines, chosen per number of rounds so the lines look centred
static int roundLineOffset(int totalRounds) {
    switch (totalRounds) {
        case 3:
        case 5:
        case 14:
            return 2;
        case 7:
        case 9:
        case 13:
            return 3;
        case 12:
            return 4;
        case 11:
            return 5;
        default:
            return 1;
    }
}

// Few rounds (less than width / 2): one line per round
static void layoutRoundLines(int currentRound, int totalRounds, int width, RoundPixel out[]) {
    int lineWidth = width / totalRounds;
    int offset = roundLineOffset(totalRounds);
    for (int i = 0; i < totalRounds; i++) {
        RoundPixel p = (i == currentRound - 1) ? ROUND_PIXEL_ACTIVE : ROUND_PIXEL_INACTIVE;
        for (int j = 0; j < lineWidth - 1; j++) {
            int x = i * lineWidth + offset + j;
            if (x >= 0 && x < width) out[x] = p;
        }
    }
}

// Many rounds: one pixel per round (at most width), the current round separated by gaps if they fit
static void layoutRoundDots(int currentRound, int totalRounds, int width, RoundPixel out[]) {
    int t = totalRounds <= width ? totalRounds : width;
    bool withGaps = totalRounds <= width - 2;
    // gaps before and after the current round, only those that are drawn (none after the last round)
    int gaps = 0;
    if (withGaps) {
        gaps = (currentRound - 1 >= 0 && currentRound - 1 < t) + (currentRound >= 0 && currentRound < t);
    }
    int start = (width - (t + gaps)) / 2;
    for (int i = 0; i < t; i++) {
        if (withGaps && (i == currentRound - 1 || i == currentRound)) {
            start++;
        }
        int x = start + i;
        if (x >= 0 && x < width) out[x] = (i == currentRound - 1) ? ROUND_PIXEL_ACTIVE : ROUND_PIXEL_INACTIVE;
    }
}

void layoutRoundIndicator(int currentRound, int totalRounds, int width, RoundPixel out[]) {
    for (int i = 0; i < width; i++) {
        out[i] = ROUND_PIXEL_OFF;
    }
    if (totalRounds <= 0) {
        return;  // number of rounds unknown, show the countdown only
    }
    if (totalRounds < width / 2) {
        layoutRoundLines(currentRound, totalRounds, width, out);
    } else {
        layoutRoundDots(currentRound, totalRounds, width, out);
    }
}
