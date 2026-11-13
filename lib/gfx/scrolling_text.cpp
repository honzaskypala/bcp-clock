// Class for scrolling text animation on Adafruit_GFX displays
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include "scrolling_text.h"
#include "fontsegment.h"

void ScrollingText::init(Adafruit_GFX *gfx, const char *text, int16_t x, int16_t y, const GFXfont *gfxFont, bool segmentedFont, int16_t color, uint16_t lpad, uint16_t rpad, bool loop, bool draw) {
    isSegFont_ = segmentedFont;
    int16_t x1;
    uint16_t w, h;
    measureText(text, &x1, &y1_, &w, &h);
    scrollCanvas_ = createCanvas(w, h, gfxFont);
    scrollCanvas_->fillScreen(0x0000);
    scrollCanvas_->setTextColor(color);
    scrollCanvas_->setCursor(0, y_ - y1_); // align baseline between gfx and canvas
    scrollCanvas_->print(text);
    totalWidth_ = lpad_ + w + rpad_;
    isActive = true;
    if (draw) {
        this->draw();
    }
}

GFXcanvas16 *ScrollingText::createCanvas(uint16_t w, uint16_t h, const GFXfont *gfxFont) {
#ifdef _UTF8_32BIT_FONT_H_
    if (isUTF8Font_) {
        UTF8canvas16 *utf8Canvas = new UTF8canvas16(w, h);
        if (isSegFont_) {
            utf8Canvas->setFont((UTF8_32BitFont *) gfxFont, isSegFont_);
        } else {
            utf8Canvas->setFont((UTF8_32BitFont *) gfxFont);
        }
        return utf8Canvas;
    }
#endif // _UTF8_32BIT_FONT_H_
    GFXcanvas16 *canvas = new GFXcanvas16(w, h);
    canvas->setFont(gfxFont);
    return canvas;
}

void ScrollingText::append(const char *text, int16_t color, bool newStart, uint16_t lpad, uint16_t rpad) {
    int16_t x1, y1;
    uint16_t w, h;
    measureText(text, &x1, &y1, &w, &h);
    int16_t new_y1 = _min(y1_, y1);
    uint16_t new_h = _max(y1_ + scrollCanvas_->height(), y1 + h) - new_y1;
#ifdef _UTF8_32BIT_FONT_H_
    UTF8canvas16 *newCanvas = new UTF8canvas16(scrollCanvas_->width() + rpad_ + lpad + w, new_h);
#else
    GFXcanvas16 *newCanvas = new GFXcanvas16(scrollCanvas_->width() + rpad_ + lpad + w, new_h);
#endif // _UTF8_32BIT_FONT_H_
    newCanvas->fillScreen(0x0000);
    newCanvas->drawRGBBitmap(0, y1_ - new_y1, scrollCanvas_->getBuffer(), scrollCanvas_->width(), scrollCanvas_->height());
    y1_ = new_y1;
#ifdef _UTF8_32BIT_FONT_H_
    if (isUTF8Font_) {
        // same font setup as the initial text, otherwise non-ASCII characters are not drawn
        newCanvas->setFont((const UTF8_32BitFont *) gfxFont_, isSegFont_);
    } else {
        newCanvas->setFont(gfxFont_);
    }
#else
    newCanvas->setFont(gfxFont_);
#endif // _UTF8_32BIT_FONT_H_
    newCanvas->setTextColor(color);
    newCanvas->setCursor(scrollCanvas_->width() + rpad_ + lpad, y_ - y1_); // baseline
    newCanvas->print(text);
    if (newStart) {
        startPos_ = scrollCanvas_->width() + rpad_ + lpad;
        newLpad_ = lpad;
    }
    rpad_ = rpad;
    totalWidth_ += lpad + w + rpad;
    delete scrollCanvas_;
    scrollCanvas_ = newCanvas;
}

void ScrollingText::step(bool draw) {
    pos_++;
    if (pos_ > totalWidth_) {
        restartOrStop();
    }
    if (draw) {
        this->draw();
    }
}

// End of the text reached: start the looped part again, or stop
void ScrollingText::restartOrStop() {
    if (!loop_) {
        isActive = false;
        return;
    }
    if (newLpad_ != -256) {
        lpad_ = newLpad_;
        newLpad_ = -256;
    }
    pos_ = startPos_;
}

void ScrollingText::draw() {
    int16_t drawPos = x_ - pos_;
    uint16_t displayWidth = gfx_->width();
    if (drawPos < displayWidth && drawPos + lpad_ > 0) {
        gfx_->fillRect(_max(drawPos, 0), y1_, _min(lpad_, lpad_ + drawPos), scrollCanvas_->height(), 0x0000);
    }
    drawPos += lpad_;
    if (drawPos < displayWidth && drawPos + scrollCanvas_->width() > 0) {
        drawBitmap(drawPos, y1_, scrollCanvas_->getBuffer(), scrollCanvas_->width(), scrollCanvas_->height(), scrollCanvas_->width());
    }
    drawPos += scrollCanvas_->width();
    if (drawPos < displayWidth && drawPos + rpad_ > 0) {    
        gfx_->fillRect(_max(drawPos, 0), y1_, _min(rpad_, rpad_ + drawPos), scrollCanvas_->height(), 0x0000);
    }
    if (loop_) {
        // next copy of the looped part: it starts at startPos_ with the left padding used after
        // the restart, so the transition to the restarted text is seamless
        uint16_t loopLpad = (newLpad_ != -256) ? newLpad_ : lpad_;
        drawPos += rpad_;
        if (drawPos < displayWidth) {
            gfx_->fillRect(_max(drawPos, 0), y1_, _min(loopLpad, loopLpad + drawPos), scrollCanvas_->height(), 0x0000);
        }
        drawPos += loopLpad;
        if (drawPos < displayWidth) {
            drawBitmap(drawPos, y1_, scrollCanvas_->getBuffer() + startPos_, scrollCanvas_->width() - startPos_,
                       scrollCanvas_->height(), scrollCanvas_->width());
        }
    }
}

void ScrollingText::drawBitmap(int16_t x, int16_t y, const uint16_t *bitmap, uint16_t w, uint16_t h, uint16_t stride) {
    gfx_->startWrite();
    for (uint16_t j = 0; j < h; j++, y++) {
        const uint16_t *row = bitmap + j * stride;
        for (uint16_t i = 0; i < w; i++) {
            gfx_->writePixel(x + i, y, row[i]);
        }
    }
    gfx_->endWrite();
}

void ScrollingText::measureText(const char *str, int16_t *x1, int16_t *y1, uint16_t *width, uint16_t *height) {
    int16_t x = x_, y = y_;
    *x1 = x_; // Initial position is value passed in
    *y1 = y_;
    *width = *height = 0;
    int16_t minx = 0x7FFF, miny = 0x7FFF, maxx = -1, maxy = -1;
    // Bound rect is intentionally initialized inverted, so 1st char sets it
#ifdef _UTF8_32BIT_FONT_H_
    utf8Char_ = 0;
    utf8BytesExpected_ = 0;
#endif // _UTF8_32BIT_FONT_H_

    uint8_t c; // Current character
    while ((c = *str++)) {
        // measureChar() modifies x/y to advance for each character,
        // and min/max x/y are updated to incrementally build bounding rect.
        measureChar(c, &x, &y, &minx, &miny, &maxx, &maxy);
    }
    if (maxx >= minx) {
        *x1 = minx;
        *width = maxx - minx + 1;
    }
    if (maxy >= miny) {
        *y1 = miny;
        *height = maxy - miny + 1;
    }
}

void ScrollingText::measureChar(const unsigned char c, int16_t *x, int16_t *y, int16_t *minx, int16_t *miny, int16_t *maxx, int16_t *maxy) {
    uint32_t cp = c;
#ifdef _UTF8_32BIT_FONT_H_
    const bool isUTF8 = isUTF8Font_;
    // invalid bytes are skipped like in UTF8canvas16::write()
    if (isUTF8 && !decodeUtf8Byte(c, utf8Char_, utf8BytesExpected_, cp)) return;
#else
    const bool isUTF8 = false;
#endif // _UTF8_32BIT_FONT_H_

    const GFXfont *font = gfxFont_;
    if (font != nullptr && isSegFont_) {
        font = findFontSegment(font, isUTF8, cp);
    }
    if (font == nullptr || font->glyph == nullptr) {
        return; // Char not found
    }

    if (cp == '\n') { // Newline?
        *x = 0;        // Reset x to zero, advance y by one line
        *y += (uint8_t) pgm_read_byte(&font->yAdvance);
        return;
    }
    if (cp == '\r') return;

    uint32_t first, last;
    fontRange(font, isUTF8, first, last);
    if (cp < first || cp > last) return; // Char not present in this font

    GFXglyph *glyph = pgm_read_glyph_ptr(font, cp - first);
    uint8_t gw = pgm_read_byte(&glyph->width),
            gh = pgm_read_byte(&glyph->height),
            xa = pgm_read_byte(&glyph->xAdvance);
    int8_t xo = pgm_read_byte(&glyph->xOffset),
           yo = pgm_read_byte(&glyph->yOffset);
    int16_t x1 = *x + xo, y1 = *y + yo, x2 = x1 + gw - 1, y2 = y1 + gh - 1;
    if (x1 < *minx) *minx = x1;
    if (y1 < *miny) *miny = y1;
    if (x2 > *maxx) *maxx = x2;
    if (y2 > *maxy) *maxy = y2;
    *x += xa;
}
