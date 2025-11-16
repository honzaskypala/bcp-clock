// Ulanzi TC001 FrameBuffer
// (c) 2025 Honza Skýpala
// WTFPL license applies

#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <Arduino.h>
#include <FastLED.h>
#include "fonts.h"

class CFrameBuffer {
public:
    static constexpr int WIDTH = 32;
    static constexpr int HEIGHT = 8;

    void show();
    void fill(CRGB c, bool show = false);
    void clear(bool show = false) { fill(CRGB::Black, show); }
    void pixel(int x, int y, CRGB color = CRGB::White, bool show = false);
    void hline(int x, int y, int length, CRGB color = CRGB::White, bool show = false);
    void vline(int x, int y, int length, CRGB color = CRGB::White, bool show = false);
    void line(int x0, int y0, int x1, int y1, CRGB color = CRGB::White, bool show = false);
    void rectangle(int x0, int y0, int x1, int y1, CRGB color = CRGB::White, bool show = false);
    void filledRectangle(int x0, int y0, int x1, int y1, CRGB color = CRGB::White, bool show = false);
    void ellipse(int x0, int y0, int rx, int ry, CRGB color = CRGB::White, bool show = false);
    void scroll(int dx, int dy, CRGB fillColor = CRGB::Black, bool show = false);
    int glyph(char c, int x, int y, String font, CRGB color = CRGB::White, bool show = false);
    void text(String str, int x, int y, String font, CRGB color = CRGB::White, bool clear = false, bool show = false);
    void textCentered(String str, int y, String font, CRGB color = CRGB::White, bool clear = false, bool show = false);

    hw_timer_t *scrollTimer = nullptr;
    void textScroll(String str, int y, String font, CRGB color = CRGB::White, CRGB bg = CRGB::Black, int lpad = WIDTH, int rpad = 0, int speed = 100, int hwtimer = 0, bool loop = true, bool clear = true);
    void textScrollStep(bool show = true);
    bool doTextScrollStep() { return _doTextScrollStep; };
    void stopTextScroll();
    void textScrollAppend(String str, CRGB color = CRGB::White, CRGB bg = CRGB::Black, int lpad = 0, int rpad = 0, bool newStart = false);

    hw_timer_t *progressTimer = nullptr;
    void progressStart(int x = 0, int y = 7, CRGB color = CRGB::White, CRGB bg = CRGB::Black, int speed = 100, int hwtimer = 1, bool show = true);
    void progressStep(bool show = true);
    bool doProgressStep() { return _doProgressStep; };
    void progressStop(bool show = true);

private:
    CFrameBuffer();
    CFrameBuffer(const CFrameBuffer&) = delete;
    CFrameBuffer& operator=(const CFrameBuffer&) = delete;

    friend CFrameBuffer& getFrameBufferInstance();

    static constexpr int NUM_LEDS = 256;
    static constexpr int DATA_PIN = 32;
    static CRGB leds[NUM_LEDS];

    void populateScrollBuffer(String str, int cursor, CRGB color);
    const bitmapfont *scrollFont = nullptr;
    CRGB *scrollBuffer = nullptr;
    long scrollBufferWidth = 0;
    long scrollOffset = 32, scrollStartPos = 0;
    int scroll_yoffset = 0;
    bool loopScrolling = false;
    bool _doTextScrollStep = false;

    int progress_pos = 0, progress_y = 0, progress_step = 1;
    CRGB progress_color = CRGB::White, progress_bg = CRGB::Black;
    bool _doProgressStep = false;
};

extern CFrameBuffer& FrameBuffer;

#endif