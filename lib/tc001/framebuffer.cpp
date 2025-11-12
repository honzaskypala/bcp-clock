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
    for (int i = 0; i < NUM_LEDS; ++i) {
        leds[i] = c;
    }
    if (show) {
        FastLED.show();
    }
}

void CFrameBuffer::pixel(int x, int y, CRGB c, bool show) {
    if (x < 0 || x >= CFrameBuffer::WIDTH || y < 0 || y >= CFrameBuffer::HEIGHT) {
        return; // Out of bounds
    }
    int index = y % 2 ? (y + 1) * CFrameBuffer::WIDTH - 1 - x : x + y * CFrameBuffer::WIDTH;
    leds[index] = c;
    if (show) {
        FastLED.show();
    }
}

void CFrameBuffer::hline(int x, int y, int length, CRGB c, bool show) {
    for (int i = 0; i < length; ++i) {
        pixel(x + i, y, c);
    }
    if (show) {
        FastLED.show();
    }
}

void CFrameBuffer::vline(int x, int y, int length, CRGB c, bool show) {
    for (int i = 0; i < length; ++i) {
        pixel(x, y + i, c);
    }
    if (show) {
        FastLED.show();
    }
}

void CFrameBuffer::line(int x0, int y0, int x1, int y1, CRGB c, bool show) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    while (true) {
        pixel(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
    if (show) {
        FastLED.show();
    }
}

void CFrameBuffer::rectangle(int x0, int y0, int x1, int y1, CRGB c, bool show) {
    hline(x0, y0, x1 - x0 + 1, c);
    hline(x0, y1, x1 - x0 + 1, c);
    vline(x0, y0, y1 - y0 + 1, c);
    vline(x1, y0, y1 - y0 + 1, c);
    if (show) {
        FastLED.show();
    }
}

void CFrameBuffer::filledRectangle(int x0, int y0, int x1, int y1, CRGB c, bool show) {
    for (int y = y0; y <= y1; ++y) {
        hline(x0, y, x1 - x0 + 1, c);
    }
    if (show) {
        FastLED.show();
    }
}

void CFrameBuffer::ellipse(int x0, int y0, int rx, int ry, CRGB c, bool show) {
    int x = -rx, y = 0;
    int err = 2 - 2 * rx;
    do {
        pixel(x0 - x, y0 + y, c);
        pixel(x0 + x, y0 + y, c);
        pixel(x0 + x, y0 - y, c);
        pixel(x0 - x, y0 - y, c);
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

    for (int y = 0; y < CFrameBuffer::HEIGHT; ++y) {
        for (int x = 0; x < CFrameBuffer::WIDTH; ++x) {
            int newX = x - dx;
            int newY = y - dy;
            if (newX >= 0 && newX < CFrameBuffer::WIDTH && newY >= 0 && newY < CFrameBuffer::HEIGHT) {
                int oldIndex = y % 2 ? (y + 1) * CFrameBuffer::WIDTH - 1 - x : x + y * CFrameBuffer::WIDTH;
                int newIndex = newY % 2 ? (newY + 1) * CFrameBuffer::WIDTH - 1 - newX : newX + newY * CFrameBuffer::WIDTH;
                temp[newIndex] = leds[oldIndex];
            }
        }
    }

    for (int i = 0; i < CFrameBuffer::WIDTH * CFrameBuffer::HEIGHT; ++i) {
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

int CFrameBuffer::glyph(char c, int x, int y, String fontName, CRGB color, bool show) {
    if (this->font == nullptr || this->currentFontName != fontName) {
        delete this->font;
        this->font = new BitmapFont(fontName);
        this->currentFontName = fontName;
    }

    uint8_t glyphData[this->font->width()];
    this->font->getGlyph(c, glyphData);

    for (int i = 0; i < this->font->width(); ++i) {
        for (int j = 0; j < this->font->height(); ++j) {
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
    for (size_t i = 0; i < str.length(); ++i) {
        char c = str.charAt(i);
        cursorX += glyph(c, cursorX, y, fontName, color) + 1;
    }

    if (show) {
        FastLED.show();
    }
}

void CFrameBuffer::textCentered(String str, int y, String fontName, CRGB color, bool clear, bool show) {
    int textWidth = this->font->textWidth(str);
    int startX = (CFrameBuffer::WIDTH - textWidth) / 2;

    text(str, startX, y, fontName, color, clear, show);
}