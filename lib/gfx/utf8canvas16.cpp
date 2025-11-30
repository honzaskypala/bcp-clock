// Class UTF8canvas16 - Adafruit_GFX 16-bit canvas supporting UTF-8 encoded text
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include "utf8canvas16.h"

void UTF8canvas16::setFont(const GFXfont *f, bool segmented) {
    isFontSegmented_ = segmented;
    GFXcanvas16::setFont(f);
}

void UTF8canvas16::setFont(const UTF8_32BitFont *utf8Font, bool segmented) {
    isFontSegmented_ = segmented && (utf8Font != nullptr);
    isUTF8Font_ = (utf8Font != nullptr);
    GFXcanvas16::setFont((GFXfont *) utf8Font);
}

size_t UTF8canvas16::write(uint8_t c) {
    const GFXfont *font = gfxFont;
    uint32_t cp = c;
    static uint32_t utf8Char = 0;
    static uint8_t utf8BytesExpected = 0;

    if (isUTF8Font_) {
        if (c < 0x80) {
            // single byte ASCII
            return GFXcanvas16::write(c);
        } else if ((c & 0xE0) == 0xC0) {
            // start of 2-byte sequence
            utf8Char = c & 0x1F;
            utf8BytesExpected = 1;
            return 0;
        } else if ((c & 0xF0) == 0xE0) {
            // start of 3-byte sequence
            utf8Char = c & 0x0F;
            utf8BytesExpected = 2;
            return 0;
        } else if ((c & 0xF8) == 0xF0) {
            // start of 4-byte sequence
            utf8Char = c & 0x07;
            utf8BytesExpected = 3;
            return 0;
        } else if ((c & 0xC0) == 0x80 && utf8BytesExpected > 0) {
            // continuation byte
            utf8Char = (utf8Char << 6) | (c & 0x3F);
            utf8BytesExpected--;
            if (utf8BytesExpected == 0) {
                cp = utf8Char;
                utf8Char = 0;
            } else {
                return 0; // still not complete
            }
        } else {
            // invalid byte, reset state
            utf8Char = 0;
            utf8BytesExpected = 0;
            return 0;
        }
    }
    // now we have a complete UTF-8 character

    if (font == nullptr) {
        return GFXcanvas16::write((char) cp);
    }

    size_t res = 0;
    while (true) {
        uint32_t first = font->first;
        uint32_t last = font->last;
        if (isUTF8Font_) {
            first = (((const UTF8_32BitFont *) font)->dFirst != 0) ? ((const UTF8_32BitFont *) font)->dFirst : first;
            last = (((const UTF8_32BitFont *) font)->dLast != 0) ? ((const UTF8_32BitFont *) font)->dLast : last;
        }
        if (cp >= first && cp <= last) {
            const GFXfont tmpFont = {
                (uint8_t  *) font->bitmap,
                (GFXglyph *) font->glyph + (cp - first),
                0,
                0,
                font->yAdvance
            };
            const GFXfont *font1 = gfxFont;
            GFXcanvas16::setFont(&tmpFont);
            c = 0;
            res = GFXcanvas16::write(c);
            GFXcanvas16::setFont(font1);
            break;
        } else if (isFontSegmented_) {
            // try next segment
            if (isUTF8Font_) {
                font = (const GFXfont *) (((const UTF8_32BitFont *) font) + 1);
            } else {
                font = font + 1;
            }
            if (font->glyph == nullptr) {
                // no more segments
                break;
            }
        } else {
            break;
        }
    }
    utf8Char = 0;
    return res;
}