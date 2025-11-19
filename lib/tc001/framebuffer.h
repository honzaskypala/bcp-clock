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
    // ---- Display dimensions ----
    static constexpr int WIDTH = 32;
    static constexpr int HEIGHT = 8;

    // ---- Basic display operations ----
    void show();
    void fill(CRGB c, bool show = false);
    inline void clear(bool show = false) { fill(CRGB::Black, show); }

    // ---- Simple geometries ----
    void pixel(int x, int y, CRGB color = CRGB::White, bool show = false);
    void line(int x0, int y0, int x1, int y1, CRGB color = CRGB::White, bool show = false);
    inline void hline(int x, int y, int length, CRGB color = CRGB::White, bool show = false) { line(x, y, x + length - 1, y, color, show); };
    inline void vline(int x, int y, int length, CRGB color = CRGB::White, bool show = false) { line(x, y, x, y + length - 1, color, show); };
    void rectangle(int x0, int y0, int x1, int y1, CRGB color = CRGB::White, bool show = false);
    void filledRectangle(int x0, int y0, int x1, int y1, CRGB color = CRGB::White, bool show = false);
    void ellipse(int x0, int y0, int rx, int ry, CRGB color = CRGB::White, bool show = false);

    // ---- Scroll display content ----
    void scroll(int dx, int dy, CRGB fillColor = CRGB::Black, bool show = false);

    // ---- Text output ----
    int glyph(char c, int x, int y, String font, CRGB color = CRGB::White, bool show = false);
    void text(String str, int x, int y, String font, CRGB color = CRGB::White, bool clear = false, bool show = false);
    void textCentered(String str, int y, String font, CRGB color = CRGB::White, bool clear = false, bool show = false);

    // ---- Scrolling text output ----
    void textScroll(String str, int y, String font, CRGB color = CRGB::White, CRGB bg = CRGB::Black, int lpad = WIDTH, int rpad = 0, int speed = 100, int hwtimer = 0, bool loop = true, bool clear = true);
    void textScrollStep(bool show = true);
    void textScrollStop();
    void textScrollAppend(String str, CRGB color = CRGB::White, CRGB bg = CRGB::Black, int lpad = 0, int rpad = 0, bool newStart = false);
    inline bool isTextScrollActive() { return isTextScrollActive_; };
    inline bool doTextScrollStep() { return doTextScrollStep_; };

    // ---- Progress indicator ----
    void progressStart(int x = 0, int y = 7, CRGB color = CRGB::White, CRGB bg = CRGB::Black, int speed = 100, int hwtimer = 1, bool show = true);
    void progressStep(bool show = true);
    void progressStop(bool show = true);
    inline bool doProgressStep() { return doProgressStep_; };

private:
    // ---- Singleton related ----
    CFrameBuffer();
    CFrameBuffer(const CFrameBuffer&) = delete;
    CFrameBuffer& operator=(const CFrameBuffer&) = delete;
    friend CFrameBuffer& getFrameBufferInstance();

    // ---- Core variables ----
    static constexpr int NUM_LEDS = 256;
    static constexpr int DATA_PIN = 32;
    static CRGB leds[NUM_LEDS];

    // ---- Text output ----
    bitmapfont const* getFont(String fontName);
    int width(char c, bitmapfont const* font);
    int width(String text, bitmapfont const* font);
    inline const uint8_t* glyphBitmapPtr(char c, bitmapfont const* font) { return &font->bitmaps[font->proportional ? c * (font->width + 1) + 1 : c * font->width]; };

    // ---- Scrolling text output ----
    volatile bool isTextScrollActive_ = false;
    volatile bool doTextScrollStep_ = false;
    CRGB *scrollBuffer = nullptr;
    long scrollBufferWidth = 0;
    long scrollOffset = 32, scrollStartPos = 0;
    int scroll_yoffset = 0;
    bool loopScrolling = false;
    const bitmapfont *scrollFont = nullptr;
    void populateScrollBuffer(String str, int cursor, CRGB color);

    // ---- Progress indicator ----
    volatile bool isProgressActive_ = false;
    volatile bool doProgressStep_ = false;
    int progress_pos = 0, progress_y = 0, progress_step = 1;
    CRGB progress_color = CRGB::White, progress_bg = CRGB::Black;

    // ---- Hardware timer for animations ----
    static constexpr int HW_TIMER = 0;
    static constexpr long HW_TIMER_PERIOD = 100; // in milliseconds
    hw_timer_t *fbTimer = nullptr;
    void ensureTimer(int hwTimer = HW_TIMER, long period = HW_TIMER_PERIOD);
    static void IRAM_ATTR onFbTimerISR();
};

// ---- Singleton instance ----
extern CFrameBuffer& FrameBuffer;

#endif