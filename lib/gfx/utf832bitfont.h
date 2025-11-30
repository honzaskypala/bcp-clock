// Structure for 32-bit Unicode codepoint font based on GFXfont
// (c) 2025 Honza Skýpala
// WTFPL license applies

#ifndef _UTF8_32BIT_FONT_H_
#define _UTF8_32BIT_FONT_H_

#include <Arduino.h>
#include <gfxfont.h>

struct UTF8_32BitFont {
    GFXfont gfxFont;               ///< GFXfont structure
    uint32_t dFirst;               ///< First Unicode codepoint in 32-bit variable
    uint32_t dLast;                ///< Last Unicode codepoint in 32-bit variable
};

#endif // _UTF8_32BIT_FONT_H_