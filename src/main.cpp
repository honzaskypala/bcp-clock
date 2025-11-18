// Round countdown clock firmware for Ulanzi TC001 desktop clock hardware,
// fetching the data from www.bestcoastpairings.com
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include <Arduino.h>
#include <framebuffer.h>
#include <config.h>
#include <wifimgr.h>
#include <bcpevent.h>
#include <time.h>

constexpr int COUNTDOWN_TIMER_ID = 3;
constexpr int COUNTDOWN_TIMER_PERIOD_US = 1000000; // 1 second
constexpr int DEBOUNCE_DELAY_MS = 500;

volatile enum {
    DISPLAY_BOOT,
    DISPLAY_SCROLL_ONCE,
    DISPLAY_EVENT_NAME,
    DISPLAY_EVENT_ROUND,
    DISPLAY_EVENT_COUNTDOWN,
    DISPLAY_EVENT_NO_UPDATE
} displayState = DISPLAY_BOOT;

hw_timer_t *countdownTimer = nullptr;
volatile bool updateCountdown = false;
volatile bool midButtonPressed = false;

void splashScreen(bool showProgress = true) {
    displayState = DISPLAY_BOOT;
    int y = showProgress ? 1 : 2;
    FrameBuffer.textScrollStop();
    FrameBuffer.progressStop();
    FrameBuffer.text("BCP", 0, y, "f3x5", CRGB::White, true);
    FrameBuffer.text("c", 14, y, "f3x5", CRGB::White);
    FrameBuffer.text("lock", 17, y, "f3x5", CRGB::White, false, !showProgress);
    if (showProgress) {
        FrameBuffer.progressStart(-1);
    }
}

void configMessage(bool loop = false) {
    FrameBuffer.textScrollStop();
    FrameBuffer.progressStop();
    FrameBuffer.textScroll("Config page http://" + WiFi.localIP().toString(), 2, "f3x5", CRGB::White, CRGB::Black, 24, loop ? 8 : 32, 100, 3, loop);
}

// ---- Time countdown using ESP-32 hardware timer ----

void ensureCountdownTimer() {
    if (countdownTimer == nullptr) {
        countdownTimer = timerBegin(COUNTDOWN_TIMER_ID, 80, true);
        timerAttachInterrupt(countdownTimer, []() { 
            if (displayState == DISPLAY_EVENT_COUNTDOWN) {
                updateCountdown = true; 
            }
        }, true);
        timerAlarmWrite(countdownTimer, COUNTDOWN_TIMER_PERIOD_US, true);
        timerAlarmEnable(countdownTimer);
    }
}

void stopCountdownTimer() {
    if (displayState == DISPLAY_EVENT_COUNTDOWN) {
        displayState = DISPLAY_BOOT;
    }
    updateCountdown = false;
}

inline void displayRound(int currentRound, int totalRounds) {
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

    long remaining = BCPEvent.roundEndEpoch() - time(nullptr);

    CRGB color = CRGB::White;
    if (remaining <= Config.redThreshold()) {
        color = CRGB::Red;
    } else if (remaining <= Config.yellowThreshold()) {
        color = CRGB::Yellow;
    }

    String font = "f4x6";
    int x = remaining < 0 ? 0 : 5,
        y = 0;
    if (BCPEvent.timerLength() > 3600) {
        font = "f3x5";
        x = remaining < 0 ? -1 : 3;
        y = 1;
    }

    int t = remaining < 0 ? -remaining : remaining;
    int hours = t / 3600;
    int minutes = (t % 3600) / 60;
    int seconds = t % 60;

    String out  = remaining < 0 ? "-" : "";;

    if (BCPEvent.timerLength() <= 3600) {
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

    displayRound(BCPEvent.currentRound(), BCPEvent.numberOfRounds());

    FrameBuffer.show();
}  

// ---- update display after data refresh ----

void displayUpdate(bool afterScrollOnce = false) {
    if (displayState == DISPLAY_SCROLL_ONCE || displayState == DISPLAY_EVENT_NO_UPDATE) {
        return;
    }
    if (BCPEvent.id() != "") {

        if (BCPEvent.ended() || !BCPEvent.started()) {
            // event either not started or already ended
            if (displayState != DISPLAY_EVENT_NAME) {
                stopCountdownTimer();
                FrameBuffer.textScroll(BCPEvent.name(), 2, "f3x5", CRGB::White, CRGB::Black, afterScrollOnce ? 32 : 24, afterScrollOnce ? 0 : 8);
                displayState = DISPLAY_EVENT_NAME;
            }

        } else if (BCPEvent.timerLength() <= 0) {
            // no timer, show round only
            if (displayState != DISPLAY_EVENT_ROUND) {
                stopCountdownTimer();
                FrameBuffer.textScrollStop();
                FrameBuffer.textCentered("Round " + String(BCPEvent.currentRound()), 2, "f3x5", CRGB::White, true, true);
                displayState = DISPLAY_EVENT_ROUND;
            }

        } else {
            // show countdown
            if (displayState != DISPLAY_EVENT_COUNTDOWN) {
                FrameBuffer.textScrollStop();
                ensureCountdownTimer();
                displayState = DISPLAY_EVENT_COUNTDOWN;
            }
        }
    }
}

// ---- event handler running on 2nd core ----

void eventHandler(void* parameter) {
    while (true) {
        if (midButtonPressed) {
            static unsigned long lastPress = millis();
            unsigned long now = millis();
            if (now - lastPress >= DEBOUNCE_DELAY_MS) {
                lastPress = now;
                midButtonPressed = false;
                stopCountdownTimer();
                FrameBuffer.textScrollStop();
                Config.startConfigServer();
                configMessage();
                displayState = DISPLAY_SCROLL_ONCE;
            }
        }
        if (Config.isConfigServerRunning()) {
            Config.handleClient();
        }
        if (displayState == DISPLAY_SCROLL_ONCE &&!FrameBuffer.isTextScrollActive()) {
            displayState = DISPLAY_BOOT;
            displayUpdate(true);
        }
        if (FrameBuffer.doTextScrollStep()) {
            FrameBuffer.textScrollStep();
        }
        if (FrameBuffer.doProgressStep()) {
            FrameBuffer.progressStep();
        }
        if (updateCountdown && displayState == DISPLAY_EVENT_COUNTDOWN) {
            countdownUpdate();
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

inline void ensureEventID() {
    // Config.erase(); // for testing only - remove in production
    if (Config.eventId() == "") {
        configMessage(true);
        Config.startConfigServer(true, 0);
        while (Config.eventId() == "") {
            delay(100); // keep server responsive
        }
        splashScreen(false);
        delay(250); // keep the server responsive for 0.25 second, let the http client receive the response
    }
    Config.stopConfigServer(); // no longer needed
}

// ---- core Arduino functions ----

void setup() {
    pinMode(15, INPUT_PULLDOWN);  // stop whistle noise
    pinMode(27, INPUT_PULLUP);    // mid button
    Serial.begin(9600);
    while (!Serial);              // wait for serial port to connect. Needed for native USB

    xTaskCreatePinnedToCore(
            eventHandler,     // Function to run
            "EventHandler",   // Task name
            4096*2,           // Stack size
            NULL,             // Parameter
            1,                // Priority
            NULL,             // Task handle
            1                 // Core (0 or 1)
        );

    splashScreen();
    WifiMgr.connect(digitalRead(27) == LOW);    // if mid button pressed, enforce portal
    ensureEventID();
    BCPEvent.setID(Config.eventId());
}

void loop() {
    static int failCount = 0;
    static time_t count = 0;
    if (Config.configUpdated) {
        Config.configUpdated = false;
        if (Config.eventId() != BCPEvent.fullId()) {
            displayState = DISPLAY_EVENT_NO_UPDATE;
            splashScreen();
            BCPEvent.setID(Config.eventId());
            BCPEvent.refreshData();
            FrameBuffer.progressStop();
            displayState = DISPLAY_BOOT;
            displayUpdate();
        }
    } else if (count % 120 == 0) {
        if (BCPEvent.refreshData()) {
            failCount = 0;
        } else {
            failCount++;
            Serial.printf("Main: Failed to refresh BCP event data (failure count %d)\n", failCount);
            if (failCount >= 5) {
                Serial.println("Main: Too many consecutive failures, restarting device");
                ESP.restart();
            }
        }
        if (count == 0) {
            FrameBuffer.progressStop();
        }
        displayState = DISPLAY_BOOT;
        displayUpdate();
    }
    if (count == 0) {
        attachInterrupt(27, []() { midButtonPressed = true; }, FALLING);
    }
    delay(500);
    count++;    
}