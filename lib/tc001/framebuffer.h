#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <FastLED.h>

class CFrameBuffer {
public:
    static constexpr int WIDTH = 32;
    static constexpr int HEIGHT = 8;

    void show();
    void fill(CRGB c, bool show = false);
    void clear(bool show = false) { fill(CRGB::Black, show); }
    void pixel(int x, int y, CRGB c = CRGB::White, bool show = false);
    void hline(int x, int y, int length, CRGB c = CRGB::White, bool show = false);
    void vline(int x, int y, int length, CRGB c = CRGB::White, bool show = false);
    void line(int x0, int y0, int x1, int y1, CRGB c = CRGB::White, bool show = false);
    void rectangle(int x0, int y0, int x1, int y1, CRGB c = CRGB::White, bool show = false);
    void filledRectangle(int x0, int y0, int x1, int y1, CRGB c = CRGB::White, bool show = false);
    void ellipse(int x0, int y0, int rx, int ry, CRGB c = CRGB::White, bool show = false);
    void scroll(int dx, int dy, CRGB fillColor = CRGB::Black, bool show = false);

private:
    CFrameBuffer();
    CFrameBuffer(const CFrameBuffer&) = delete;
    CFrameBuffer& operator=(const CFrameBuffer&) = delete;

    static constexpr int NUM_LEDS = 256;
    static constexpr int DATA_PIN = 32;
    static CRGB leds[NUM_LEDS];

    friend CFrameBuffer& getFrameBufferInstance();
};

extern CFrameBuffer& FrameBuffer;

#endif