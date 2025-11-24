#pragma once
#include <Print.h>
#include "framebuffer.h"
#include "wifimgr.h"

class WifiMgrMsg : public Print {
private:
    char buffer[256];
    size_t bufferIndex = 0;

public:
    size_t write(uint8_t c) override {
        if (c == '\n' || bufferIndex >= sizeof(buffer) - 1) {
            buffer[bufferIndex] = '\0';
            if (strncmp(buffer, "Error:", 6) == 0) {
                FrameBuffer.progressStop();
                FrameBuffer.textScrollStop();
                FrameBuffer.textScroll(buffer, 2, "p3x5", CRGB::Red, CRGB::Black, 24);
                if (initMsg_.length() != 0) {
                    FrameBuffer.textScrollAppend(initMsg_, CRGB::White, CRGB::Black, 24, 8, true);
                }
            } else if (strncmp(buffer, "Connecting ", 11) == 0) {
                FrameBuffer.progressStart(-1);
                FrameBuffer.textScrollStop();
                FrameBuffer.textCentered("Connect", 1, "p3x5", CRGB::White, true);
            } else if (strncmp(buffer, "Connected ", 10) == 0) {
                // do nothing, let the current status message continue
            } else if (WifiMgr.timeSyncFailed()) {
                FrameBuffer.progressStop();
                FrameBuffer.textScrollAppend(buffer, CRGB::White, CRGB::Black, 24, 8, true);
                if (initMsg_.length() == 0) initMsg_ = String(buffer);
            } else {
                FrameBuffer.progressStop();
                FrameBuffer.textScrollStop();
                FrameBuffer.textScroll(buffer, 2, "p3x5", CRGB::White, CRGB::Black, 24);
                if (initMsg_.length() == 0) initMsg_ = String(buffer);
            }
            bufferIndex = 0;
        } else if (c != '\r' && bufferIndex < sizeof(buffer) - 2) {
            buffer[bufferIndex++] = c;
        }
        return 1;
    }

private:
    String initMsg_ = String();
};