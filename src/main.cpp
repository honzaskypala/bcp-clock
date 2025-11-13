#include <Arduino.h>
#include <LittleFS.h>
#include <framebuffer.h>
#include <config.h>

void setup() {
    pinMode(15, INPUT_PULLDOWN);
    Serial.begin(9600);

    if (!LittleFS.begin()) {
        Serial.println("LittleFS mount failed");
        FrameBuffer.fill(CRGB::Red, true);
        return;
    }

    // splash screen
    FrameBuffer.text("BCP", 0, 1, "f3x5", CRGB::White, true);
    FrameBuffer.text("c", 14, 1, "f3x5", CRGB::White);
    FrameBuffer.text("lock", 17, 1, "f3x5", CRGB::White, false, true);

    delay(5000);
    FrameBuffer.textScroll("Hello, World!", 2, "f3x5", CRGB::Green, CRGB::Black, 16, 16);
    Serial.println(Config.getHostname());
}

void loop() {
    delay(10);

    if (FrameBuffer.doTextScrollStep()) {
        FrameBuffer.textScrollStep();
    }
}