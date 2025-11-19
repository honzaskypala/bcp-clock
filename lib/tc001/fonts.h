// Bitmap fonts for Ulanzi TC001 FrameBuffer
// (c) 2025 Honza Skýpala
// WTFPL license applies

#ifndef TC001_BITMAPFONTS_H
#define TC001_BITMAPFONTS_H

#include <Arduino.h>

struct bitmapfont {
    uint8_t width, height;
    bool proportional;
    uint32_t firstChar, lastChar;
    const uint8_t *bitmaps;
};

#include "p3x5.h"
#include "p3x5-00410.h"
#include "p3x5-02010.h"
#include "p3x5-1f1e6.h"
#include "p4x6.h"
#include "f3x5.h"
#include "f4x6.h"

static const bitmapfont FONTS[] PROGMEM = {
    {3, 5, true,        0,     383, P3X5_00000_BITMAPS},   // p3x5 proportional
    {3, 5, true,  0x00410, 0x0045F, P3X5_00410_BITMAPS},   // p3x5 cyrillic
    {3, 5, true,  0x02010, 0x0205F, P3X5_02010_BITMAPS},   // p3x5 general punctuation
    {3, 5, true,  0x1F1E6, 0x1F1FF, P3X5_1F1E6_BITMAPS},   // p3x5 flag emojis
    {3, 5, false,       0,     127, F3X5_00000_BITMAPS},   // f3x5 fixed width
    {4, 6, true,        0,     127, P4X6_00000_BITMAPS},   // p4x6 proportional
    {4, 6, false,       0,     127, F4X6_00000_BITMAPS},   // f4x6 fixed width
    {0, 0, false, 0, 0, nullptr}                           // End marker
};

#endif