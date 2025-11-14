#include <Arduino.h>
#include <LittleFS.h>
#include <framebuffer.h>
#include <config.h>
#include <wifimgr.h>
#include <bcpevent.h>
#include <time.h>

enum {
    DISPLAY_BOOT,
    DISPLAY_SCROLL_ONCE,
    DISPLAY_EVENT_NAME,
    DISPLAY_EVENT_ROUND,
    DISPLAY_EVENT_COUNTDOWN
} displayState = DISPLAY_BOOT;

hw_timer_t *countdownTimer = nullptr;
bool updateCountdown = false;

void stopCountdownTimer() {
    if (countdownTimer != nullptr) {
        timerEnd(countdownTimer);
        countdownTimer = nullptr;
    }
    updateCountdown = false;
}

void displayUpdate() {
    if (displayState == DISPLAY_SCROLL_ONCE) {
        return;
    }
    if (BCPEvent.getID() != "") {

        if (BCPEvent.isEnded() || !BCPEvent.isStarted()) {
            // event either not started or already ended
            if (displayState != DISPLAY_EVENT_NAME) {
                stopCountdownTimer();
                FrameBuffer.stopTextScroll();
                FrameBuffer.textScroll(BCPEvent.getName(), 2, "f3x5", CRGB::Green, CRGB::Black, 16, 16);
                displayState = DISPLAY_EVENT_NAME;
            }

        } else if (BCPEvent.getTimerLength() <= 0) {
            // no timer, show round only
            if (displayState != DISPLAY_EVENT_ROUND) {
                stopCountdownTimer();
                FrameBuffer.stopTextScroll();
                FrameBuffer.textCentered("Round " + String(BCPEvent.getCurrentRound()), 2, "f3x5", CRGB::White, true, true);
                displayState = DISPLAY_EVENT_ROUND;
            }

        } else {
            // show countdown
            if (displayState != DISPLAY_EVENT_COUNTDOWN) {
                FrameBuffer.stopTextScroll();
                countdownTimer = timerBegin(3, 80, true);
                timerAttachInterrupt(countdownTimer, []() { updateCountdown = true; }, true);
                timerAlarmWrite(countdownTimer, 1000000, true);
                timerAlarmEnable(countdownTimer);
                displayState = DISPLAY_EVENT_COUNTDOWN;
            }
        }
    }
}

void displayRound(int currentRound, int totalRounds) {
    CRGB activeColor = CRGB::DeepSkyBlue,
         inactiveColor = 0x0f0f0f;
    const int line = 7;
    FrameBuffer.hline(0, line, CFrameBuffer::WIDTH, CRGB::Black, false);
    if (totalRounds < FrameBuffer.WIDTH / 2) {
        int lineWidth = CFrameBuffer::WIDTH / totalRounds, offset;
        switch (totalRounds) {
            case 3:
            case 5:
            case 14:
                offset = 2;
                break;
            case 7:
            case 9:
            case 13:
                offset = 3;
                break;
            case 12:
                offset = 4;
                break;
            case 11:
                offset = 5;
                break;
            default:
                offset = 1;
        }
        for (int i = 0; i < totalRounds; i++) {
            FrameBuffer.hline(i * lineWidth + offset, line, lineWidth - 1, (i == currentRound - 1) ? activeColor : inactiveColor, false);
        }
    } else {
        int t = totalRounds <= FrameBuffer.WIDTH ? totalRounds : FrameBuffer.WIDTH;
        int start = (FrameBuffer.WIDTH - (t + (t <= FrameBuffer.WIDTH - 2 ? 2 : 0))) / 2;
        for (int i = 0; i < t; i++) {
            if (totalRounds <= FrameBuffer.WIDTH - 2 && (i == currentRound - 1 || i == currentRound)) {
                start++;
            }
            FrameBuffer.pixel(start + i, line, (i == currentRound - 1) ? activeColor : inactiveColor, false);
        }
    }
}

void countdownUpdate() {
    if (!updateCountdown) {
        return;
    }
    updateCountdown = false;

    long remaining = BCPEvent.getRoundEndEpoch() - time(nullptr);

    CRGB color = CRGB::White;
    if (remaining <= Config.getRedThreshold()) {
        color = CRGB::Red;
    } else if (remaining <= Config.getYellowThreshold()) {
        color = CRGB::Yellow;
    }

    String font = "f4x6";
    int x = remaining < 0 ? 0 : 5,
        y = 0;
    if (BCPEvent.getTimerLength() > 3600) {
        font = "f3x5";
        x = remaining < 0 ? -1 : 3;
        y = 1;
    }

    int t = remaining < 0 ? -remaining : remaining;
    int hours = t / 3600;
    int minutes = (t % 3600) / 60;
    int seconds = t % 60;

    String out  = remaining < 0 ? "-" : "";;

    if (BCPEvent.getTimerLength() <= 3600) {
        // mm:ss
        if (remaining < -3599) {
            out = "-XX:XX";
        } else {
            out += String(minutes / 10) + String(minutes % 10) + ":" +
                   String(seconds / 10) + String(seconds % 10);
        }
    } else {
        // h:mm:ss
        out += String(hours) + ":" +
               String(minutes / 10) + String(minutes % 10) + ":" +
               String(seconds / 10) + String(seconds % 10);
    }

    FrameBuffer.text(out, x, y, font, color, true, false);

    displayRound(BCPEvent.getCurrentRound(), BCPEvent.getNumberOfRounds());

    FrameBuffer.show();
}   

void displayRefresh(void* parameter) {
    while (true) {
        if (FrameBuffer.doTextScrollStep()) {
            FrameBuffer.textScrollStep();
        }
        if (FrameBuffer.doProgressStep()) {
            FrameBuffer.progressStep();
        }
        if (updateCountdown) {
            countdownUpdate();
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
    FrameBuffer.text("lock", 17, 1, "f3x5", CRGB::White);
    FrameBuffer.progressStart();

    WifiMgr.connect();
    BCPEvent.setID("https://www.bestcoastpairings.com/organize/event/HW0pjCBn5deR?active_tab=roster");
    BCPEvent.refreshData();

    FrameBuffer.progressStop();
}

void loop() {
    displayUpdate();
    delay(60000);
    BCPEvent.refreshData();
}
