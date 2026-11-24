// Display brightness: config page value and LED level, independent of the hardware
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include "brightness.h"

// ---- Display brightness ----

uint8_t normalizeBrightness(long percent) {
    if (percent < BRIGHTNESS_MIN) return BRIGHTNESS_MIN;
    if (percent > BRIGHTNESS_MAX) return BRIGHTNESS_MAX;
    return (percent + BRIGHTNESS_STEP / 2) / BRIGHTNESS_STEP * BRIGHTNESS_STEP;
}

uint8_t brightnessLevel(uint8_t percent) {
    if (percent > BRIGHTNESS_MAX) percent = BRIGHTNESS_MAX;
    return percent * 255 / BRIGHTNESS_MAX;
}
