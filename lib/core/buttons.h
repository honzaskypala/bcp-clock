// Button presses: debouncing, short and long press, independent of the hardware
// (c) 2025 Honza Skýpala
// WTFPL license applies

#pragma once

#include <stdint.h>

enum ButtonEvent : uint8_t {
    BUTTON_NONE,
    BUTTON_LEFT,       // pressed
    BUTTON_RIGHT,      // pressed
    BUTTON_MID_SHORT,  // released before MID_LONG_PRESS_MS
    BUTTON_MID_LONG    // held for MID_LONG_PRESS_MS, reported without waiting for the release
};

constexpr unsigned long BUTTON_DEBOUNCE_MS = 30;    // a level must be stable this long to count
constexpr unsigned long MID_LONG_PRESS_MS = 1000;

enum PressKind : uint8_t {
    PRESS_NONE,
    PRESS_DOWN,   // button went down (debounced)
    PRESS_SHORT,  // released before longPressMs
    PRESS_LONG    // held for longPressMs (once per press)
};

// One button sampled periodically (every 10 ms). Every press reports PRESS_DOWN, then either PRESS_SHORT
// at the release or PRESS_LONG while still held.
class PressDetector {
public:
    // pressedAtStart: the button is already down (e.g. held at power-on), that press is ignored
    explicit PressDetector(bool pressedAtStart = false, unsigned long longPressMs = MID_LONG_PRESS_MS)
        : longPressMs_(longPressMs), stable_(pressedAtStart), raw_(pressedAtStart), ignore_(pressedAtStart) {}

    PressKind update(bool pressed, unsigned long nowMs);

private:
    unsigned long longPressMs_;
    bool stable_;              // debounced level
    bool raw_;                 // last sampled level
    unsigned long rawSince_ = 0;
    unsigned long downAt_ = 0;
    bool started_ = false;     // rawSince_ valid
    bool longReported_ = false;
    bool ignore_;              // current press is ignored until released
};
