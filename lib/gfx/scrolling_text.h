// Class for scrolling text animation on Adafruit_GFX displays
// (c) 2025 Honza Skýpala
// WTFPL license applies

#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <animation.h>
#include <utf8canvas16.h>
#include <utf832bitfont.h>

#define DEFAULT_LPAD 32
#define DEFAULT_RPAD 0
#define DEFAULT_COLOR 0xFFFF
#define DEFAULT_LOOP true
#define DEFAULT_DRAW true

class ScrollingText : public GFXanimation {
public:
    ScrollingText(Adafruit_GFX *gfx, const char *text, int16_t x, int16_t y, const GFXfont *gfxFont, bool segmentedFont = false, int16_t color = DEFAULT_COLOR, uint16_t lpad = DEFAULT_LPAD, uint16_t rpad = DEFAULT_RPAD, bool loop = DEFAULT_LOOP, bool draw = DEFAULT_DRAW) : GFXanimation(gfx), x_(x), y_(y), gfxFont_(gfxFont), lpad_(lpad), rpad_(rpad), loop_(loop), startPos_(0), pos_(0), newLpad_(-256) {
        _ScrollingText(gfx, text, x, y, gfxFont, segmentedFont, color, lpad, rpad, loop, draw);
    };
    ScrollingText(Adafruit_GFX *gfx, const String &text, int16_t x, int16_t y, const GFXfont *gfxFont, bool segmentedFont = false, int16_t color = DEFAULT_COLOR, uint16_t lpad = DEFAULT_LPAD, uint16_t rpad = DEFAULT_RPAD, bool loop = DEFAULT_LOOP, bool draw = DEFAULT_DRAW) : ScrollingText(gfx, text.c_str(), x, y, gfxFont, segmentedFont, color, lpad, rpad, loop, draw) {};

#ifdef _UTF8_32BIT_FONT_H_
    ScrollingText(Adafruit_GFX *gfx, const char *text, int16_t x, int16_t y, const UTF8_32BitFont *gfxFont, bool segmentedFont = false, int16_t color = DEFAULT_COLOR, uint16_t lpad = DEFAULT_LPAD, uint16_t rpad = DEFAULT_RPAD, bool loop = DEFAULT_LOOP, bool draw = DEFAULT_DRAW) : GFXanimation(gfx), x_(x), y_(y), gfxFont_((const GFXfont *) gfxFont), lpad_(lpad), rpad_(rpad), loop_(loop), startPos_(0), pos_(0), newLpad_(-256), isSegFont_(true) {
        isUTF8Font_ = true;
        _ScrollingText(gfx, text, x, y, (const GFXfont *) gfxFont, segmentedFont, color, lpad, rpad, loop, draw);
    };
    ScrollingText(Adafruit_GFX *gfx, const String &text, int16_t x, int16_t y, const UTF8_32BitFont *gfxFont, bool segmentedFont = false, int16_t color = DEFAULT_COLOR, uint16_t lpad = DEFAULT_LPAD, uint16_t rpad = DEFAULT_RPAD, bool loop = DEFAULT_LOOP, bool draw = DEFAULT_DRAW) : ScrollingText(gfx, text.c_str(), x, y, gfxFont, segmentedFont, color, lpad, rpad, loop, draw) {};
#endif // _UTF8_32BIT_FONT_H_

    ~ScrollingText() {
        if (scrollCanvas_ != nullptr) delete scrollCanvas_;
    };

    void append(const char *text, int16_t color = DEFAULT_COLOR, bool newStart = false, uint16_t lpad = DEFAULT_LPAD, uint16_t rpad = DEFAULT_RPAD);
    void append(const String &text, int16_t color = DEFAULT_COLOR, bool newStart = false, uint16_t lpad = DEFAULT_LPAD, uint16_t rpad = DEFAULT_RPAD) {
        append(text.c_str(), color, newStart, lpad, rpad);
    }
    void step(bool draw = DEFAULT_DRAW) override;
    void draw() override;

private:
    const GFXfont *gfxFont_;
    bool isSegFont_ = false;
#ifdef _UTF8_32BIT_FONT_H_
    bool isUTF8Font_ = false;
#endif // _UTF8_32BIT_FONT_H_
    GFXcanvas16 *scrollCanvas_;
    int16_t x_;
    int16_t y_;   // text baseline as per Adafruit GFX print text methods
    int16_t y1_;  // scroll canvas top y position
    uint16_t lpad_, rpad_;
    int16_t newLpad_;
    uint16_t totalWidth_;
    uint16_t startPos_, pos_;
    bool loop_;

    void _ScrollingText(Adafruit_GFX *gfx, const char *text, int16_t x, int16_t y, const GFXfont *gfxFont, bool segmentedFont, int16_t color, uint16_t lpad, uint16_t rpad, bool loop, bool draw);

    // We need to implement our own textDimensions() and charBounds() functions, as the
    // Adafruit GFX versions clips the text to the display size, which we do not want here.
    // We need full dimensions of the text even if it is larger than the display.
    void textDimensions(const char *str, int16_t *x1, int16_t *y1, uint16_t *width, uint16_t *height);
    void charBounds(const unsigned char c, int16_t *x, int16_t *y, int16_t *minx, int16_t *miny, int16_t *maxx, int16_t *maxy);

    // we need our own drawBitmap(), as the methods in Adafruit GFX work only
    // with 1-bit bitmaps, not with 16-bit color bitmaps as used in our canvas
    void drawBitmap(int16_t x, int16_t y, uint16_t *bitmap, uint16_t w, uint16_t h);

    inline GFXglyph *pgm_read_glyph_ptr(const GFXfont *gfxFont, uint8_t c) {
#ifdef __AVR__
        return &(((GFXglyph *)pgm_read_pointer(&gfxFont->glyph))[c]);
#else
        // expression in __AVR__ section may generate "dereferencing type-punned
        // pointer will break strict-aliasing rules" warning In fact, on other
        // platforms (such as STM32) there is no need to do this pointer magic as
        // program memory may be read in a usual way So expression may be simplified
        return gfxFont->glyph + c;
#endif //__AVR__
    }
};