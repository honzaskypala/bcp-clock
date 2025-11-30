// Class for scrolling text animation on Adafruit_GFX displays
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include "scrolling_text.h"

void ScrollingText::_ScrollingText(Adafruit_GFX *gfx, const char *text, int16_t x, int16_t y, const GFXfont *gfxFont, bool segmentedFont, int16_t color, uint16_t lpad, uint16_t rpad, bool loop, bool draw) {
    isSegFont_ = segmentedFont;
    int16_t x1;
    uint16_t w, h;
    textDimensions(text, &x1, &y1_, &w, &h);
#ifdef _UTF8_32BIT_FONT_H_
    if (isUTF8Font_) {
        UTF8canvas16 *utf8Canvas = new UTF8canvas16(w, h);
        if (isSegFont_) {
            utf8Canvas->setFont((UTF8_32BitFont *) gfxFont, isSegFont_);
        } else {
            utf8Canvas->setFont((UTF8_32BitFont *) gfxFont);
        }
        scrollCanvas_ = utf8Canvas;
    } else {
#endif // _UTF8_32BIT_FONT_H_
        scrollCanvas_ = new GFXcanvas16(w, h);
        scrollCanvas_->setFont(gfxFont);
#ifdef _UTF8_32BIT_FONT_H_
    }
#endif // _UTF8_32BIT_FONT_H_
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

void ScrollingText::append(const char *text, int16_t color, bool newStart, uint16_t lpad, uint16_t rpad) {
    int16_t x1, y1;
    uint16_t w, h;
    textDimensions(text, &x1, &y1, &w, &h);
    int16_t new_y1 = _min(y1_, y1);
    uint16_t new_h = _max(y1_ + scrollCanvas_->height(), y1 + h) - new_y1;
    canvasType *newCanvas = new canvasType(scrollCanvas_->width() + rpad_ + lpad + w, new_h);
    newCanvas->fillScreen(0x0000);
    newCanvas->drawRGBBitmap(0, y1_ - new_y1, scrollCanvas_->getBuffer(), scrollCanvas_->width(), scrollCanvas_->height());
    y1_ = new_y1;
    newCanvas->setFont(gfxFont_);
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
        if (loop_) {
            if (newLpad_ != -256) {
                lpad_ = newLpad_;
                newLpad_ = -256;
            }
            pos_ = startPos_;
        } else {
            isActive = false;
        }
    }
    if (draw) {
        this->draw();
    }
}

void ScrollingText::draw() {
    int16_t drawPos = x_ - pos_;
    uint16_t displayWidth = gfx_->width();
    if (drawPos < displayWidth && drawPos + lpad_ > 0) {
        gfx_->fillRect(_max(drawPos, 0), y1_, _min(lpad_, lpad_ + drawPos), scrollCanvas_->height(), 0x0000);
    }
    drawPos += lpad_;
    if (drawPos < displayWidth && drawPos + scrollCanvas_->width() > 0) {
        drawBitmap(drawPos, y1_, scrollCanvas_->getBuffer(), scrollCanvas_->width(), scrollCanvas_->height());
    }
    drawPos += scrollCanvas_->width();
    if (drawPos < displayWidth && drawPos + rpad_ > 0) {    
        gfx_->fillRect(_max(drawPos, 0), y1_, _min(rpad_, rpad_ + drawPos), scrollCanvas_->height(), 0x0000);
    }
    if (loop_) {
        drawPos += rpad_;
        if (drawPos < displayWidth) {
            gfx_->fillRect(_max(drawPos, 0), y1_, _min(lpad_, lpad_ + drawPos), scrollCanvas_->height(), 0x0000);
        }
        drawPos += lpad_;
        if (drawPos < displayWidth) {
            drawBitmap(drawPos, y1_, scrollCanvas_->getBuffer(), _max(scrollCanvas_->width(), displayWidth), scrollCanvas_->height());
        }
    }
}

void ScrollingText::drawBitmap(int16_t x, int16_t y, uint16_t *bitmap, uint16_t w, uint16_t h) {
    gfx_->startWrite();
    for (uint16_t j = 0; j < h; j++, y++) {
        for (uint16_t i = 0; i < w; i++) {
            gfx_->writePixel(x + i, y, *bitmap++);
        }
    }
    gfx_->endWrite();
}

void ScrollingText::textDimensions(const char *str, int16_t *x1, int16_t *y1, uint16_t *width, uint16_t *height) {
    int16_t x = x_, y = y_;
    *x1 = x_; // Initial position is value passed in
    *y1 = y_;
    *width = *height = 0;
    int16_t minx = 0x7FFF, miny = 0x7FFF, maxx = -1, maxy = -1;
    // Bound rect is intentionally initialized inverted, so 1st char sets it

    uint8_t c; // Current character
    while ((c = *str++)) {
        // charBounds() modifies x/y to advance for each character,
        // and min/max x/y are updated to incrementally build bounding rect.
        charBounds(c, &x, &y, &minx, &miny, &maxx, &maxy);
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

void ScrollingText::charBounds(const unsigned char c, int16_t *x, int16_t *y, int16_t *minx, int16_t *miny, int16_t *maxx, int16_t *maxy) {
    uint32_t cp = c;

    if (isUTF8Font_) {
        // handle UTF-8 decoding
        static uint32_t utf8Char = 0;
        static uint8_t utf8BytesExpected = 0;
        if ((c & 0xE0) == 0xC0) { // 2-byte sequence
            utf8Char = c & 0x1F;
            utf8BytesExpected = 1;
            return;
        } else if ((c & 0xF0) == 0xE0) { // 3-byte sequence
            utf8Char = c & 0x0F;
            utf8BytesExpected = 2;
            return;
        } else if ((c & 0xF8) == 0xF0) { // 4-byte sequence
            utf8Char = c & 0x07;
            utf8BytesExpected = 3;
            return;
        } else if ((c & 0xC0) == 0x80 && utf8BytesExpected > 0) { // continuation byte
            utf8Char = (utf8Char << 6) | (c & 0x3F);
            utf8BytesExpected--;
            if (utf8BytesExpected == 0) {
                // complete character
                cp = utf8Char;
                utf8Char = 0;
            } else {
                return; // still not complete
            }
        }
    }

    const GFXfont *font = gfxFont_;

    // handle segmented fonts
    if (font != nullptr && isSegFont_) {
        while (font->glyph != nullptr) {
            uint32_t first = font->first;
            uint32_t last = font->last;
            if (isUTF8Font_) {
                const UTF8_32BitFont *utf8Font = (const UTF8_32BitFont *) font;
                first = (utf8Font->dFirst != 0) ? utf8Font->dFirst : first;
                last = (utf8Font->dLast != 0) ? utf8Font->dLast : last;
            }
            if (cp >= first && cp <= last) {
                break;
            } else {
                // try next segment
                if (isUTF8Font_) {
                    font = (const GFXfont *) (((const UTF8_32BitFont *) font) + 1);
                } else {
                    font = font + 1;
                }
            }
        }
    }

    if (font == nullptr || font->glyph == nullptr) {
        return; // Char not found
    }

    if (cp == '\n') { // Newline?
        *x = 0;        // Reset x to zero, advance y by one line
        *y += (uint8_t) pgm_read_byte(&font->yAdvance);
    } else if (cp != '\r') { // Not a carriage return; is normal char
        uint32_t first = pgm_read_word(&font->first),
                 last = pgm_read_word(&font->last);
        if (isUTF8Font_) {
            first = (((const UTF8_32BitFont *) font)->dFirst != 0) ? ((const UTF8_32BitFont *) font)->dFirst : first;
            last = (((const UTF8_32BitFont *) font)->dLast != 0) ? ((const UTF8_32BitFont *) font)->dLast : last;
        }
        if ((cp >= first) && (cp <= last)) { // Char present in this font?
            GFXglyph *glyph = pgm_read_glyph_ptr(font, cp - first);
            uint8_t gw = pgm_read_byte(&glyph->width),
                    gh = pgm_read_byte(&glyph->height),
                    xa = pgm_read_byte(&glyph->xAdvance);
            int8_t xo = pgm_read_byte(&glyph->xOffset),
                    yo = pgm_read_byte(&glyph->yOffset);
            // if (wrap && ((*x + (((int16_t)xo + gw) * textsize_x)) > _width)) {
            //     *x = 0; // Reset x to zero, advance y by one line
            //     *y += textsize_y * (uint8_t)pgm_read_byte(&gfxFont->yAdvance);
            // }
            int16_t tsx = 1, tsy = 1,
                    x1 = *x + xo * tsx, y1 = *y + yo * tsy, x2 = x1 + gw * tsx - 1,
                    y2 = y1 + gh * tsy - 1;
            if (x1 < *minx)
                *minx = x1;
            if (y1 < *miny)
                *miny = y1;
            if (x2 > *maxx)
                *maxx = x2;
            if (y2 > *maxy)
                *maxy = y2;
            *x += xa * tsx;
        }
    }
}
