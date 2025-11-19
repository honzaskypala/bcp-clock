// Bitmap fonts for Ulanzi TC001 FrameBuffer
// (c) 2025 Honza Skýpala
// WTFPL license applies

#ifndef TC001_BITMAPFONTS_H
#define TC001_BITMAPFONTS_H

#include <Arduino.h>

struct bitmapfont {
    uint8_t width;
    uint8_t height;
    bool proportional;
    int page;
    unsigned int lastChar;
    const uint8_t *bitmaps;
};

#include "p3x5.h"
#include "p4x6.h"
#include "f3x5.h"
#include "f4x6.h"

static const bitmapfont FONTS[] PROGMEM = {
    {3, 5, true,  0, 127, P3X5_PAGE00_BITMAPS},  // p3x5 proportional
    {3, 5, false, 0, 127, F3X5_PAGE00_BITMAPS},  // f3x5 fixed width
    {4, 6, true,  0, 127, P4X6_PAGE00_BITMAPS},  // p4x6 proportional
    {4, 6, false, 0, 127, F4X6_PAGE00_BITMAPS},  // f4x6 fixed width
    {0, 0, false, 0, 0, nullptr} // End marker
};

#endif