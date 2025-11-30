// Class UTF8canvas16 - Adafruit_GFX 16-bit canvas supporting UTF-8 encoded text
// (c) 2025 Honza Skýpala
// WTFPL license applies

#ifndef _UTF8CANVAS16_H_
#define _UTF8CANVAS16_H_

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <utf832bitfont.h>

class UTF8canvas16 : public GFXcanvas16 {
public:
    UTF8canvas16(int16_t w, int16_t h) : GFXcanvas16(w, h) {}
    size_t write(uint8_t c) override;
    void setFont(const GFXfont *f = NULL, bool segmented = false);
    void setFont(const UTF8_32BitFont *utf8Font, bool segmented = false);

private:
    bool isFontSegmented_ = false;
    bool isUTF8Font_ = false;
};

#endif // _UTF8CANVAS16_H_