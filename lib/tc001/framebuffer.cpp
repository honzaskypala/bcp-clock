#include "framebuffer.h"

CRGB CFrameBuffer::leds[CFrameBuffer::NUM_LEDS];

CFrameBuffer::CFrameBuffer() {
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
}

CFrameBuffer& getFrameBufferInstance() {
    static CFrameBuffer instance;
    return instance;
}

CFrameBuffer& FrameBuffer = getFrameBufferInstance();

void CFrameBuffer::show() {
    FastLED.show();
}

void CFrameBuffer::fill(CRGB c, bool show) {
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = c;
    }
    if (show) {
        FastLED.show();
    }
}

void CFrameBuffer::pixel(int x, int y, CRGB color, bool show) {
    if (x < 0 || x >= CFrameBuffer::WIDTH || y < 0 || y >= CFrameBuffer::HEIGHT) {
        return; // Out of bounds
    }
    int index = y % 2 ? (y + 1) * CFrameBuffer::WIDTH - 1 - x : x + y * CFrameBuffer::WIDTH;
    leds[index] = color;
    if (show) {
        FastLED.show();
    }
}

void CFrameBuffer::hline(int x, int y, int length, CRGB color, bool show) {
    for (int i = 0; i < length; i++) {
        pixel(x + i, y, color);
    }
    if (show) {
        FastLED.show();
    }
}

void CFrameBuffer::vline(int x, int y, int length, CRGB color, bool show) {
    for (int i = 0; i < length; i++) {
        pixel(x, y + i, color);
    }
    if (show) {
        FastLED.show();
    }
}

void CFrameBuffer::line(int x0, int y0, int x1, int y1, CRGB color, bool show) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    while (true) {
        pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
    if (show) {
        FastLED.show();
    }
}

void CFrameBuffer::rectangle(int x0, int y0, int x1, int y1, CRGB color, bool show) {
    hline(x0, y0, x1 - x0 + 1, color);
    hline(x0, y1, x1 - x0 + 1, color);
    vline(x0, y0, y1 - y0 + 1, color);
    vline(x1, y0, y1 - y0 + 1, color);
    if (show) {
        FastLED.show();
    }
}

void CFrameBuffer::filledRectangle(int x0, int y0, int x1, int y1, CRGB color, bool show) {
    for (int y = y0; y <= y1; y++) {
        hline(x0, y, x1 - x0 + 1, color);
    }
    if (show) {
        FastLED.show();
    }
}

void CFrameBuffer::ellipse(int x0, int y0, int rx, int ry, CRGB color, bool show) {
    int x = -rx, y = 0;
    int err = 2 - 2 * rx;
    do {
        pixel(x0 - x, y0 + y, color);
        pixel(x0 + x, y0 + y, color);
        pixel(x0 + x, y0 - y, color);
        pixel(x0 - x, y0 - y, color);
        int e2 = err;
        if (e2 <= y) {
            err += ++y * 2 + 1;
            if (-x == y && e2 <= x) e2 = 0;
        }
        if (e2 > x) err += ++x * 2 + 1;
    } while (x <= 0);
    if (show) {
        FastLED.show();
    }
}

void CFrameBuffer::scroll(int dx, int dy, CRGB fillColor, bool show) {
    CRGB temp[CFrameBuffer::WIDTH * CFrameBuffer::HEIGHT];

    for (int y = 0; y < CFrameBuffer::HEIGHT; y++) {
        for (int x = 0; x < CFrameBuffer::WIDTH; x++) {
            int newX = x - dx;
            int newY = y - dy;
            if (newX >= 0 && newX < CFrameBuffer::WIDTH && newY >= 0 && newY < CFrameBuffer::HEIGHT) {
                int oldIndex = y % 2 ? (y + 1) * CFrameBuffer::WIDTH - 1 - x : x + y * CFrameBuffer::WIDTH;
                int newIndex = newY % 2 ? (newY + 1) * CFrameBuffer::WIDTH - 1 - newX : newX + newY * CFrameBuffer::WIDTH;
                temp[newIndex] = leds[oldIndex];
            }
        }
    }

    for (int i = 0; i < CFrameBuffer::WIDTH * CFrameBuffer::HEIGHT; i++) {
        if (temp[i] == CRGB(0, 0, 0)) {
            leds[i] = fillColor;
        } else {
            leds[i] = temp[i];
        }
    }

    if (show) {
        FastLED.show();
    }
}

void CFrameBuffer::loadFont(String fontName) {
    if (this->font == nullptr || this->currentFontName != fontName) {
        delete this->font;
        this->font = new BitmapFont(fontName);
        this->currentFontName = fontName;
    }
}

int CFrameBuffer::glyph(char c, int x, int y, String fontName, CRGB color, bool show) {
    this->loadFont(fontName);
    uint8_t glyphData[this->font->width()];
    this->font->getGlyph(c, glyphData);

    for (int i = 0; i < this->font->width(); i++) {
        for (int j = 0; j < this->font->height(); j++) {
            if (glyphData[i] & (1 << j)) {
                pixel(x + i, y + j, color);
            }
        }
    }

    if (show) {
        FastLED.show();
    }

    return this->font->width();
}

void CFrameBuffer::text(String str, int x, int y, String fontName, CRGB color, bool clear, bool show) {
    if (clear) {
        this->clear();
    }

    int cursorX = x;
    for (size_t i = 0; i < str.length(); i++) {
        char c = str.charAt(i);
        cursorX += glyph(c, cursorX, y, fontName, color) + 1;
    }

    if (show) {
        FastLED.show();
    }
}

void CFrameBuffer::textCentered(String str, int y, String fontName, CRGB color, bool clear, bool show) {
    this->loadFont(fontName);
    int textWidth = this->font->textWidth(str);
    int startX = (CFrameBuffer::WIDTH - textWidth) / 2;

    text(str, startX, y, fontName, color, clear, show);
}

void CFrameBuffer::textScroll(String str, int y, String fontName, CRGB color, CRGB bg, int lpad, int rpad, int speed, int hwtimer, bool loop, bool clear) {
    this->loadFont(fontName);
    int textWidth = this->font->textWidth(str);
    this->scrollBufferWidth = lpad + textWidth + rpad;
    this->scroll_yoffset = y;

    if (this->scrollBuffer != nullptr) {
        delete[] this->scrollBuffer;
    }
    this->scrollBuffer = new CRGB[this->scrollBufferWidth * this->font->height()];

    for (int i = 0; i < this->scrollBufferWidth * this->font->height(); i++) {
        this->scrollBuffer[i] = bg;
    }

    int cursor = lpad * this->font->height();
    for (size_t i = 0; i < str.length(); i++) {
        char c = str.charAt(i);
        uint8_t char_width = this->font->width();
        uint8_t glyphData[char_width];
        this->font->getGlyph(c, glyphData);

        for (int gx = 0; gx < char_width; gx++) {
            for (int gy = 0; gy < this->font->height(); gy++, cursor++) {
                if (glyphData[gx] & (1 << gy)) {
                    this->scrollBuffer[cursor] = color;
                }
            }
        }
        cursor += this->font->height();
    }

    if (clear) {
        this->clear();
    }

    for (int i = 0; i < this->scrollBufferWidth * this->font->height(); i++) {
        this->pixel(i / this->font->height(), i % this->font->height() + y, this->scrollBuffer[i]);
    }

    this->show();

    this->scrollTimer = timerBegin(hwtimer, 80, true);
    timerAttachInterrupt(this->scrollTimer, []() { FrameBuffer._doTextScrollStep = true; }, true);
    timerAlarmWrite(this->scrollTimer, speed * 1000, true);
    timerAlarmEnable(this->scrollTimer);

    this->loopScrolling = loop;
    this->scrollStartPos = 0;
}

void CFrameBuffer::textScrollStep(bool show) {
    static int offset = 32;
    this->scroll(1, 0, CRGB::Black, false);
    for (int sy = 0; sy < this->font->height(); sy++) {
        int bufferIndex = offset * this->font->height() + sy;
        pixel(CFrameBuffer::WIDTH - 1, sy + this->scroll_yoffset, this->scrollBuffer[bufferIndex]);
    }
    if (show) {
        this->show();
    }
    this->_doTextScrollStep = false;
    offset++;
    if (offset >= this->scrollBufferWidth) {
        if (this->loopScrolling) {
            offset = this->scrollStartPos;
        } else {
            this->stopTextScroll();
        }
    }
}

void CFrameBuffer::stopTextScroll() {
    if (this->scrollTimer != nullptr) {
        timerAlarmDisable(this->scrollTimer);
        timerEnd(this->scrollTimer);
        this->scrollTimer = nullptr;
    }
    if (this->scrollBuffer != nullptr) {
        delete[] this->scrollBuffer;
        this->scrollBuffer = nullptr;
    }
    this->_doTextScrollStep = false;
}

void CFrameBuffer::textScrollAppend(String str, CRGB color, CRGB bg, int lpad, int rpad, bool newStart) {
    if (this->font == nullptr) {
        return;
    }

    int oldWidth = this->scrollBufferWidth;
    int textWidth = lpad + this->font->textWidth(str) + rpad;
    this->scrollBufferWidth += textWidth;

    CRGB *newBuffer = new CRGB[this->scrollBufferWidth * this->font->height()];

    for (int i = 0; i < this->scrollBufferWidth * this->font->height(); i++) {
        newBuffer[i] = bg;
    }

    for (int i = 0; i < oldWidth * this->font->height(); i++) {
        newBuffer[i] = this->scrollBuffer[i];
    }

    int cursor = (oldWidth + lpad) * this->font->height();
    for (size_t i = 0; i < str.length(); i++) {
        char c = str.charAt(i);
        uint8_t char_width = this->font->width();
        uint8_t glyphData[char_width];
        this->font->getGlyph(c, glyphData);

        for (int gx = 0; gx < char_width; gx++) {
            for (int gy = 0; gy < this->font->height(); gy++, cursor++) {
                if (glyphData[gx] & (1 << gy)) {
                    newBuffer[cursor] = color;
                }
            }
        }
        cursor += this->font->height();
    }

    delete[] this->scrollBuffer;
    this->scrollBuffer = newBuffer;

    if (newStart) {
        this->scrollStartPos = oldWidth;
    }
}

void CFrameBuffer::progressStart(int x, int y, CRGB color, CRGB bg, int speed, int hwtimer, bool show) {
    if (x >= 0)
        this->progress_pos = x;
    this->progress_y = y;
    this->progress_color = color;

    if (show) {
        this->progressStep(show);
    }

    this->progressTimer = timerBegin(hwtimer, 80, true);
    timerAttachInterrupt(this->progressTimer, []() { FrameBuffer._doProgressStep = true; }, true);
    timerAlarmWrite(this->progressTimer, speed * 1000, true);
    timerAlarmEnable(this->progressTimer);
}

void CFrameBuffer::progressStep(bool show) {
    this->pixel(this->progress_pos, this->progress_y, progress_bg, false);
    if ((this->progress_step >= 0 && this->progress_pos + this->progress_step >= CFrameBuffer::WIDTH) || (this->progress_step < 0 && this->progress_pos + this->progress_step < 0)) {
        this->progress_step = -this->progress_step;
    }
    this->progress_pos += this->progress_step;
    this->pixel(this->progress_pos, this->progress_y, this->progress_color, show);
    this->_doProgressStep = false;
}

void CFrameBuffer::progressStop(bool show) {
    if (this->progressTimer != nullptr) {
        timerAlarmDisable(this->progressTimer);
        timerEnd(this->progressTimer);
        this->progressTimer = nullptr;
    }
    this->_doProgressStep = false;
    this->pixel(this->progress_pos, this->progress_y, progress_bg, show);
}