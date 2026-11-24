// Display brightness: config page value and LED level, independent of the hardware
// (c) 2025 Honza Skýpala
// WTFPL license applies

#pragma once

#include <stdint.h>

// ---- Display brightness ----

constexpr uint8_t BRIGHTNESS_MIN = 10;    // percent; lower values are raised to it
constexpr uint8_t BRIGHTNESS_MAX = 100;   // percent, full brightness
constexpr uint8_t BRIGHTNESS_STEP = 10;   // percent

// Brightness in percent clamped to BRIGHTNESS_MIN..BRIGHTNESS_MAX and rounded to the nearest step
uint8_t normalizeBrightness(long percent);

// LED brightness 0..255 of a brightness in percent (100 % = 255)
uint8_t brightnessLevel(uint8_t percent);
