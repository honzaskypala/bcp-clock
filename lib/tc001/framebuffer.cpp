// Ulanzi TC001 FrameBuffer
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include "framebuffer.h"

CRGB CFrameBuffer::leds[CFrameBuffer::NUM_LEDS];

// ---- Constructor: init hardware ----

CFrameBuffer::CFrameBuffer() {
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
}

// ---- Basic display operations ----

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

// ---- Simple geometries ----

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
    vline(x1, y0, y1 - y0 + 1, color, show);
}

void CFrameBuffer::filledRectangle(int x0, int y0, int x1, int y1, CRGB color, bool show) {
    for (int y = y0; y < y1; y++) {
        hline(x0, y, x1 - x0 + 1, color);
    }
    hline(x0, y1, x1 - x0 + 1, color, show);
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

// ---- Scroll display content ----

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

// ---- Text output ----

int CFrameBuffer::glyph(char c, int x, int y, String fontName, CRGB color, bool show) {
    const bitmapfont* font = getFont(fontName);
    if (!font) {
        return 0; // No matching font found
    }
    if (c < 0 || c > font->lastChar) {
        c = 0; // Character out of bounds
    }
    int w = width(c, font);
    const uint8_t* glyphBitmap = glyphBitmapPtr(c, font);
    for (int i = 0; i < w; i++, glyphBitmap++) {
        for (int j = 0; j < font->height; j++) {
            if (*glyphBitmap & (1 << j)) {
                pixel(x + i, y + j, color);
            }
        }
    }
    if (show) {
        FastLED.show();
    }
    return w;
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
    bitmapfont const* font = getFont(fontName);
    if (!font) {
        return; // No matching font found
    }
    int textWidth = width(str, font);
    int startX = (CFrameBuffer::WIDTH - textWidth) / 2;

    text(str, startX, y, fontName, color, clear, show);
}

// ---- Scrolling text output ----

void CFrameBuffer::textScroll(String str, int y, String fontName, CRGB color, CRGB bg, int lpad, int rpad, int speed, int hwtimer, bool loop, bool clear) {
    if (isTextScrollActive_) {
        textScrollStop();
    }

    scrollFont = getFont(fontName);
    if (!scrollFont) {
        return; // No matching font found
    }
    int textWidth = width(str, scrollFont);
    scrollBufferWidth = lpad + textWidth + rpad;
    scroll_yoffset = y;

    if (scrollBuffer != nullptr) {
        delete[] scrollBuffer;
    }
    scrollBuffer = new CRGB[scrollBufferWidth * scrollFont->height];

    for (int i = 0; i < scrollBufferWidth * scrollFont->height; i++) {
        scrollBuffer[i] = bg;
    }

    populateScrollBuffer(str, lpad * scrollFont->height, color);
    if (clear) {
        this->clear();
    }

    for (int i = 0; i < scrollBufferWidth * scrollFont->height; i++) {
        pixel(i / scrollFont->height, i % scrollFont->height + y, scrollBuffer[i]);
    }

    show();
    loopScrolling = loop;
    scrollStartPos = 0;
    scrollOffset = 32;
    ensureTimer();
    isTextScrollActive_ = true;
}

void CFrameBuffer::textScrollStep(bool show) {
    scroll(1, 0, CRGB::Black, false);
    for (int sy = 0; sy < scrollFont->height; sy++) {
        int bufferIndex = scrollOffset * scrollFont->height + sy;
        pixel(CFrameBuffer::WIDTH - 1, sy + scroll_yoffset, scrollBuffer[bufferIndex]);
    }
    if (show) {
        this->show();
    }
    doTextScrollStep_ = false;
    scrollOffset++;
    if (scrollOffset >= scrollBufferWidth) {
        if (loopScrolling) {
            scrollOffset = scrollStartPos;
        } else {
            textScrollStop();
        }
    }
}

void CFrameBuffer::textScrollStop() {
    isTextScrollActive_ = false;
    doTextScrollStep_ = false;
    if (scrollBuffer != nullptr) {
        delete[] scrollBuffer;
        scrollBuffer = nullptr;
    }
}

void CFrameBuffer::textScrollAppend(String str, CRGB color, CRGB bg, int lpad, int rpad, bool newStart) {
    if (scrollFont == nullptr) {
        return;
    }
    isTextScrollActive_ = false;

    int oldWidth = scrollBufferWidth;
    int textWidth = width(str, scrollFont);
    scrollBufferWidth += lpad + textWidth + rpad;

    CRGB *newBuffer = new CRGB[scrollBufferWidth * scrollFont->height];

    for (int i = 0; i < scrollBufferWidth * scrollFont->height; i++) {
        newBuffer[i] = bg;
    }

    for (int i = 0; i < oldWidth * scrollFont->height; i++) {
        newBuffer[i] = scrollBuffer[i];
    }
    delete[] scrollBuffer;
    scrollBuffer = newBuffer;

    populateScrollBuffer(str, (oldWidth + lpad) * scrollFont->height, color);

    if (newStart) {
        scrollStartPos = oldWidth;
    }

    isTextScrollActive_ = true;
}

bitmapfont const* CFrameBuffer::getFont(String fontName) {
    int w = 0, h = 0;
    int sep = fontName.indexOf('x');
    if (sep > 1) {
        w = fontName.substring(1, sep).toInt();
        h = fontName.substring(sep + 1).toInt();
    } else {
        return nullptr; // Invalid font name
    }
    for (size_t i = 0;; ++i) {
        const auto& f = FONTS[i];
        if (f.width == 0 || f.height == 0) break; // sentinel
        if (f.width == w && f.height == h && ((f.proportional && fontName.charAt(0) == 'p') || (!f.proportional && fontName.charAt(0) == 'f'))) {
            return &f;
        }
    }
    return nullptr; // No matching font found
}

int CFrameBuffer::width(char c, bitmapfont const* font) {
    if (c < 0 || c > font->lastChar) {
        c = 0; // Character out of bounds
    }
    if (font->proportional) {
        // For proportional fonts, first byte is width
        uint8_t charWidth = pgm_read_byte(&font->bitmaps[c * (font->width + 1)]);
        return charWidth;
    } else {
        return font->width;
    }
}

int CFrameBuffer::width(String text, bitmapfont const* font) {
    int textWidth = 0;
    for (size_t i = 0; i < text.length(); i++) {
        textWidth += width(text.charAt(i), font) + 1;
    }
    if (text.length() > 0) {
        textWidth--; // Remove last added space
    }
    return textWidth;
}

void CFrameBuffer::populateScrollBuffer(String str, int cursor, CRGB color) {
    for (size_t i = 0; i < str.length(); i++) {
        char c = str.charAt(i);
        if (c < 0 || c > scrollFont->lastChar) {
            c = 0; // Character out of bounds
        }
        uint8_t charWidth = width(c, scrollFont);
        const uint8_t* glyphBitmap = glyphBitmapPtr(c, scrollFont);
        for (int gx = 0; gx < charWidth; gx++, glyphBitmap++) {
            for (int gy = 0; gy < scrollFont->height; gy++, cursor++) {
                if (*glyphBitmap & (1 << gy)) {
                    scrollBuffer[cursor] = color;
                }
            }
        }
        cursor += scrollFont->height;
    }
}

// ---- Progress indicator ----

void CFrameBuffer::progressStart(int x, int y, CRGB color, CRGB bg, int speed, int hwtimer, bool show) {
    isProgressActive_ = false;
    doProgressStep_ = false;

    if (x >= 0)
        progress_pos = x;
    progress_y = y;
    progress_color = color;

    if (show) {
        progressStep(show);
    }

    ensureTimer();
    isProgressActive_ = true;
}

void CFrameBuffer::progressStep(bool show) {
    pixel(progress_pos, progress_y, progress_bg, false);
    if ((progress_step >= 0 && progress_pos + progress_step >= CFrameBuffer::WIDTH) || (progress_step < 0 && progress_pos + progress_step < 0)) {
        progress_step = -progress_step;
    }
    progress_pos += progress_step;
    pixel(progress_pos, progress_y, progress_color, show);
    doProgressStep_ = false;
}

void CFrameBuffer::progressStop(bool show) {
    if (isProgressActive_) {
        pixel(progress_pos, progress_y, progress_bg, show);
    }
    isProgressActive_ = false;
    doProgressStep_ = false;
}

// ---- Hardware timer for animations ----

void CFrameBuffer::ensureTimer(int hwTimer, long period) {
    if (fbTimer == nullptr) {
        fbTimer = timerBegin(hwTimer, 80, true);
        timerAttachInterrupt(fbTimer, &CFrameBuffer::onFbTimerISR, true);
        timerAlarmWrite(fbTimer, period * 1000, true);
        timerAlarmEnable(fbTimer);
    }
}

void IRAM_ATTR CFrameBuffer::onFbTimerISR() {
    // NOTE: Keep ISR as short as possible
    CFrameBuffer* self = &FrameBuffer; // use the global instance, avoid function call in ISR
    if (!self) {
        return;
    }
    if (self->isTextScrollActive_) {
        self->doTextScrollStep_ = true;
    }
    if (self->isProgressActive_) {
        self->doProgressStep_ = true;
    }
}

// ---- Singleton related ----

CFrameBuffer& getFrameBufferInstance() {
    static CFrameBuffer instance;
    return instance;
}

CFrameBuffer& FrameBuffer = getFrameBufferInstance();
