// Class for progress indicator animation on Adafruit_GFX displays
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include "progress_indicator.h"

ProgressIndicator::ProgressIndicator(Adafruit_GFX *gfx, int16_t x, int16_t y, int16_t w, int16_t color, int16_t bgColor) : GFXanimation(gfx), x_(x), y_(y), w_(w), color_(color), bgColor_(bgColor) {
    isActive = true;
};

void ProgressIndicator::step(bool draw) {
    gfx_->drawPixel(x_ + position_, y_, bgColor_);
    if (position_ + stepSize_ >= w_ || position_ + stepSize_ < 0) {
        stepSize_ = -stepSize_;
    }
    position_ += stepSize_;
    if (draw) {
        this->draw();
    }
}

void ProgressIndicator::draw() {
    gfx_->drawPixel(x_ + position_, y_, color_);
}

void ProgressIndicator::hide() {
    gfx_->drawPixel(x_ + position_, y_, bgColor_);
    isActive = false;
}