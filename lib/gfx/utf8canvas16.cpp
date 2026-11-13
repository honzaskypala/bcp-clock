// Class UTF8canvas16 - Adafruit_GFX 16-bit canvas supporting UTF-8 encoded text
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include "utf8canvas16.h"
#include "fontsegment.h"

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
    uint32_t cp = c;
    if (isUTF8Font_) {
        if (c < 0x80) return GFXcanvas16::write(c);  // single byte ASCII
        if (!decodeUtf8Byte(c, utf8Char_, utf8BytesExpected_, cp)) return 0;
    }
    // now we have a complete UTF-8 character

    const GFXfont *font = gfxFont;
    if (font == nullptr) {
        return GFXcanvas16::write((char) cp);
    }
    utf8Char_ = 0;
    if (isFontSegmented_) {
        font = findFontSegment(font, isUTF8Font_, cp);
        if (font == nullptr) return 0;
    }
    uint32_t first, last;
    fontRange(font, isUTF8Font_, first, last);
    if (cp < first || cp > last) return 0;

    // draw the glyph through a one-glyph font, as GFXcanvas16 handles only 8-bit characters
    const GFXfont tmpFont = {
        (uint8_t  *) font->bitmap,
        (GFXglyph *) font->glyph + (cp - first),
        0,
        0,
        font->yAdvance
    };
    const GFXfont *font1 = gfxFont;
    GFXcanvas16::setFont(&tmpFont);
    size_t res = GFXcanvas16::write((uint8_t) 0);
    GFXcanvas16::setFont(font1);
    return res;
}
