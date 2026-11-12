// Abstract class for animations on Adafruit_GFX displays
// (c) 2025 Honza Skýpala
// WTFPL license applies

#pragma once

#include <Adafruit_GFX.h>

class GFXanimation {
public:
    GFXanimation(Adafruit_GFX *gfx) : gfx_(gfx) {};
    virtual ~GFXanimation() = default;
    virtual void step(bool draw = true);
    virtual void draw();
    bool isActive = false;

protected:
    Adafruit_GFX *gfx_;
};