// Button presses: debouncing, short and long press, independent of the hardware
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include "buttons.h"

PressKind PressDetector::update(bool pressed, unsigned long nowMs) {
    if (!started_ || pressed != raw_) {
        raw_ = pressed;
        rawSince_ = nowMs;
        started_ = true;
    }

    if (raw_ != stable_ && nowMs - rawSince_ >= BUTTON_DEBOUNCE_MS) {
        stable_ = raw_;
        if (stable_) {
            downAt_ = nowMs;
            longReported_ = false;
            return PRESS_DOWN;
        }
        bool wasIgnored = ignore_;
        ignore_ = false;
        if (wasIgnored || longReported_) return PRESS_NONE;
        return PRESS_SHORT;
    }

    if (stable_ && !ignore_ && !longReported_ && nowMs - downAt_ >= longPressMs_) {
        longReported_ = true;
        return PRESS_LONG;
    }
    return PRESS_NONE;
}
