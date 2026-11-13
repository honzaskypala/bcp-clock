// UTF-8 decoding and segmented font lookup shared by UTF8canvas16 and ScrollingText
// (c) 2025 Honza Skýpala
// WTFPL license applies

#pragma once

#include <Arduino.h>
#include <gfxfont.h>
#include <utf832bitfont.h>

// Feeds one byte to the UTF-8 decoder (state in acc/expected). Returns true when cp holds a complete
// code point: an ASCII byte (state untouched) or the last continuation byte. Invalid bytes reset the state.
inline bool decodeUtf8Byte(uint8_t c, uint32_t& acc, uint8_t& expected, uint32_t& cp) {
    if (c < 0x80) {
        cp = c;
        return true;
    }
    if ((c & 0xE0) == 0xC0) {           // start of 2-byte sequence
        acc = c & 0x1F;
        expected = 1;
        return false;
    }
    if ((c & 0xF0) == 0xE0) {           // start of 3-byte sequence
        acc = c & 0x0F;
        expected = 2;
        return false;
    }
    if ((c & 0xF8) == 0xF0) {           // start of 4-byte sequence
        acc = c & 0x07;
        expected = 3;
        return false;
    }
    if ((c & 0xC0) != 0x80 || expected == 0) {  // invalid byte
        acc = 0;
        expected = 0;
        return false;
    }
    acc = (acc << 6) | (c & 0x3F);      // continuation byte
    if (--expected > 0) return false;   // still not complete
    cp = acc;
    acc = 0;
    return true;
}

// Code point range of a font (segment); a UTF8_32BitFont overrides it with dFirst/dLast when set
inline void fontRange(const GFXfont *font, bool isUTF8, uint32_t& first, uint32_t& last) {
    first = font->first;
    last = font->last;
    if (!isUTF8) return;
    const UTF8_32BitFont *utf8Font = (const UTF8_32BitFont *) font;
    if (utf8Font->dFirst != 0) first = utf8Font->dFirst;
    if (utf8Font->dLast != 0) last = utf8Font->dLast;
}

inline const GFXfont *nextSegment(const GFXfont *font, bool isUTF8) {
    return isUTF8 ? (const GFXfont *) (((const UTF8_32BitFont *) font) + 1) : font + 1;
}

// Segment of a segmented font (array terminated by an entry with glyph == nullptr) containing cp,
// nullptr if there is none
inline const GFXfont *findFontSegment(const GFXfont *font, bool isUTF8, uint32_t cp) {
    for (; font->glyph != nullptr; font = nextSegment(font, isUTF8)) {
        uint32_t first, last;
        fontRange(font, isUTF8, first, last);
        if (cp >= first && cp <= last) return font;
    }
    return nullptr;
}
