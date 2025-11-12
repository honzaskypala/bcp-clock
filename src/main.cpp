#include <Arduino.h>
#include <framebuffer.h>
#include <LittleFS.h>

void setup() {
    pinMode(15, INPUT_PULLDOWN);
    Serial.begin(9600);

    if (!LittleFS.begin()) {
        Serial.println("LittleFS mount failed");
        FrameBuffer.fill(CRGB::Red, true);
        return;
    }
    FrameBuffer.text("BCP", 0, 1, "f3x5", CRGB::White, true, false);
    FrameBuffer.text("c", 14, 1, "f3x5", CRGB::White, false, false);
    FrameBuffer.text("lock", 17, 1, "f3x5", CRGB::White, false, true);
}

void loop() {
    delay(5000);
    FrameBuffer.textCentered("HELLO", 1, "f3x5", CRGB::White, true, true);
}