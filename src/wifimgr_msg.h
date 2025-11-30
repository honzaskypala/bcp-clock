#pragma once
#include <Print.h>
#include <FastLED_NeoMatrix.h>
#include <progress_indicator.h>
#include <scrolling_text.h>
#include "wifimgr.h"

#define COLOR_WHITE matrix.Color24to16(CRGB::White)
#define COLOR_BLACK matrix.Color24to16(CRGB::Black)
#define COLOR_RED matrix.Color24to16(CRGB::Red)

extern FastLED_NeoMatrix matrix;
extern void stopAnimations();
extern void ensureAnimationsTimer();
extern ProgressIndicator *progressIndicator;
extern ScrollingText *scrollingText;

class WifiMgrMsg : public Print {
private:
    char buffer[256];
    size_t bufferIndex;
    const GFXfont *font_;
    int16_t x_, y_;

public:
    WifiMgrMsg(const GFXfont &font, int16_t x = 0, int16_t y = 7) : Print(), bufferIndex(0), font_(&font), x_(x), y_(y) {}
    size_t write(uint8_t c) override {
        if (c == '\n' || bufferIndex >= sizeof(buffer) - 1) {
            buffer[bufferIndex] = '\0';
            if (strncmp(buffer, "Error:", 6) == 0) {
                // Error message, display in red
                stopAnimations();
                ensureAnimationsTimer();
                matrix.clear();
                scrollingText = new ScrollingText(&matrix, buffer, x_, y_, (const GFXfont *) font_, false, COLOR_RED);
                if (initMsg_.length() > 0) {
                    scrollingText->append(initMsg_, COLOR_WHITE, true);
                } else {
                    errorShown_ = true;
                }
            } else if (strncmp(buffer, "Connecting ", 11) == 0) {
                // Connecting message, show progress indicator
                stopAnimations();
                matrix.clear();
                matrix.setCursor(2, 6);
                matrix.setTextColor(COLOR_WHITE);
                matrix.print("Connect");
                ensureAnimationsTimer();
                progressIndicator = new ProgressIndicator(&matrix, -1, matrix.height() - 1, matrix.width(), COLOR_WHITE, COLOR_BLACK);
            } else if (strncmp(buffer, "Connected ", 10) == 0) {
                // do nothing, let the current status message continue
            } else {
                if (errorShown_) {
                    scrollingText->append(buffer, COLOR_WHITE, true);
                    errorShown_ = false;
                } else {
                    stopAnimations();
                    matrix.clear();
                    ensureAnimationsTimer();
                    scrollingText = new ScrollingText(&matrix, buffer, x_, y_, font_);
                }
                matrix.print(buffer);
                if (initMsg_.length() == 0) initMsg_ = String(buffer);
            }
            matrix.show();
            bufferIndex = 0;
        } else if (c != '\r' && bufferIndex < sizeof(buffer) - 2) {
            buffer[bufferIndex++] = c;
        }
        return 1;
    }

private:
    String initMsg_ = String();
    bool errorShown_ = false;
};