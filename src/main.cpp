#include <Arduino.h>
#include <LittleFS.h>
#include <framebuffer.h>
#include <config.h>
#include <wifimgr.h>
#include <bcpevent.h>

void displayRefresh(void* parameter) {
    while (1) {
        if (FrameBuffer.doTextScrollStep()) {
            FrameBuffer.textScrollStep();
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void setup() {
    pinMode(15, INPUT_PULLDOWN);
    Serial.begin(9600);

    xTaskCreatePinnedToCore(
            displayRefresh,   // Function to run
            "DisplayRefresh", // Task name
            4096,             // Stack size
            NULL,             // Parameter
            1,                // Priority
            NULL,             // Task handle
            1                 // Core (0 or 1)
        );

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

    WifiMgr.connect();

    BCPEvent.setID("https://www.bestcoastpairings.com/event/IaDn1zzGAq5b");
    Serial.println("Event ID: " + BCPEvent.getID());
    BCPEvent.refreshData();
    Serial.println("Event Name: " + BCPEvent.name);
    Serial.println("Event Started: " + String(BCPEvent.started));
    Serial.println("Event Ended: " + String(BCPEvent.ended));
    Serial.println("Number of Rounds: " + String(BCPEvent.numberOfRounds));
    Serial.println("Current Round: " + String(BCPEvent.currentRound));

    FrameBuffer.textScroll(BCPEvent.name, 2, "f3x5", CRGB::Green, CRGB::Black, 16, 16);
}

void loop() {
    delay(60000);
}
