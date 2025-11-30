// Class for progress indicator animation on Adafruit_GFX displays
// (c) 2025 Honza Skýpala
// WTFPL license applies

#pragma once

#include <animation.h>

class ProgressIndicator : public GFXanimation {
public:
    ProgressIndicator(Adafruit_GFX *gfx, int16_t x, int16_t y, int16_t w, int16_t color = 0xFFFF, int16_t bgColor = 0x0000);
    void step(bool draw = true) override;
    void draw() override;
    void hide();

private:
    int16_t x_, y_, w_;
    int16_t position_ = 0;
    int16_t stepSize_ = 1;
    int16_t color_;
    int16_t bgColor_;
};