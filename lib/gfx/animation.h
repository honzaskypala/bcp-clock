// Abstract class for animations on Adafruit_GFX displays
// (c) 2025 Honza Skýpala
// WTFPL license applies

#pragma once

#include <Adafruit_GFX.h>

class GFXanimation {
public:
    GFXanimation(Adafruit_GFX *gfx) : gfx_(gfx) {};
    virtual void step(bool draw = true);
    virtual void draw();
    bool isActive;

protected:
    Adafruit_GFX *gfx_;
};