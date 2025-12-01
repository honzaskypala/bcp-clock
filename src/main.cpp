// Round countdown clock firmware for Ulanzi TC001 desktop clock hardware,
// fetching the data from www.bestcoastpairings.com
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include <Arduino.h>
#include <config.h>
#include <wifimgr.h>
#include <bcpevent.h>
#include <time.h>

#include <Adafruit_GFX.h>
#include <FastLED.h>
#include <FastLED_NeoMatrix.h>
#include <progress_indicator.h>
#include <scrolling_text.h>
#include "wifimgr_msg.h"

#include <f3x5.h>
#include <f4x6.h>

#define PIN_BUZZER          15
#define PIN_LED_MATRIX      32

CRGB matrixleds[256];
FastLED_NeoMatrix matrix(matrixleds, 32, 8, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG );

constexpr int COUNTDOWN_TIMER_ID = 3;
constexpr int COUNTDOWN_TIMER_PERIOD_US = 1000000; // 1 second
constexpr int DEBOUNCE_DELAY_MS = 500;

#define COLOR_WHITE matrix.Color24to16(CRGB::White)
#define COLOR_BLACK matrix.Color24to16(CRGB::Black)
#define COLOR_RED matrix.Color24to16(CRGB::Red)
#define COLOR_YELLOW matrix.Color24to16(CRGB::Yellow)
#define COLOR_GREEN matrix.Color24to16(CRGB::Green)
#define COLOR_MAGENTA matrix.Color24to16(CRGB::Magenta)
#define COLOR_DEEPSKYBLUE matrix.Color24to16(CRGB::DeepSkyBlue)
#define COLOR_DARKGRAY matrix.Color(20, 20, 20)

hw_timer_t *animationsTimer = nullptr;
ProgressIndicator *progressIndicator = nullptr;
volatile bool updateProgress = false;
ScrollingText *scrollingText = nullptr;
volatile bool updateScrollingText = false;

Print *debugOut_ = nullptr;  // Optional debug output
#define MAIN_DEBUG(msg) if (debugOut_) { debugOut_->print("[Main] "); debugOut_->println(msg); }

const UTF8_32BitFont *defaultFont = &F3x5[0];
const GFXfont *hmmssFont = &F3x5_Fixed;
const GFXfont *mmssFont = &F4x6;

enum DisplayState {
    DISPLAY_BOOT,
    DISPLAY_SCROLL_ONCE,
    DISPLAY_EVENT_NAME,
    DISPLAY_EVENT_ROUND,
    DISPLAY_EVENT_COUNTDOWN,
    DISPLAY_EVENT_NO_UPDATE,
    DISPLAY_INVALID_EVENT
};
volatile DisplayState displayState = DISPLAY_BOOT;

#define DISPLAY_STATE (displayState == DISPLAY_EVENT_COUNTDOWN ? "COUNTDOWN" : \
                       displayState == DISPLAY_EVENT_NAME ? "EVENT_NAME" : \
                       displayState == DISPLAY_EVENT_ROUND ? "EVENT_ROUND" : \
                       displayState == DISPLAY_SCROLL_ONCE ? "SCROLL_ONCE" : \
                       displayState == DISPLAY_INVALID_EVENT ? "INVALID_EVENT" : \
                       displayState == DISPLAY_BOOT ? "BOOT" : \
                       displayState == DISPLAY_EVENT_NO_UPDATE ? "EVENT_NO_UPDATE" : "UNKNOWN")

hw_timer_t *countdownTimer = nullptr;
volatile bool updateCountdown = false;
volatile bool midButtonPressed = false;

void ensureAnimationsTimer() {
    if (animationsTimer == nullptr) {
        animationsTimer = timerBegin(2, 80, true);
        timerAttachInterrupt(animationsTimer, []() {
            if (progressIndicator && progressIndicator->isActive) {
                updateProgress = true;
            }
            if (scrollingText && scrollingText->isActive) {
                updateScrollingText = true;
            }
        }, true);
        timerAlarmWrite(animationsTimer, 100000, true); // 10 Hz
        timerAlarmEnable(animationsTimer);
    }
}

void stopAnimations() {
    if (scrollingText && scrollingText->isActive) {
        scrollingText->isActive = false;
        delete scrollingText;
        scrollingText = nullptr;
    }
    if (progressIndicator && progressIndicator->isActive) {
        progressIndicator->hide();
        progressIndicator->isActive = false;
    }
}

void splashScreen(bool showProgress = true) {
    displayState = DISPLAY_BOOT;
    stopAnimations();
    matrix.clear();
    int y = showProgress ? 6 : 7;
    matrix.setCursor(0, y);
    matrix.setTextColor(COLOR_WHITE);
    matrix.print("BCP");
    int16_t x1, y1;
    uint16_t w, h;
    const String msg = "clock";
    matrix.getTextBounds(msg, 0, y, &x1, &y1, &w, &h);
    matrix.setCursor((matrix.width() - w), y);  
    matrix.print("clock");
    if (showProgress) {
        if (progressIndicator == nullptr) {
            ensureAnimationsTimer();
            progressIndicator = new ProgressIndicator(&matrix, 0, matrix.height() - 1, matrix.width(), COLOR_WHITE, COLOR_BLACK);
        }
        progressIndicator->isActive = true;
    }
    matrix.show();
}

void configMessage(bool loop = false) {
    stopAnimations();
    matrix.clear();
    ensureAnimationsTimer();
    scrollingText = new ScrollingText(&matrix, "Config page http://" + WiFi.localIP().toString(), 0, 7, (const GFXfont *) defaultFont, false, COLOR_WHITE, 24, 8, loop);
    matrix.show();
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
    uint16_t activeColor = COLOR_DEEPSKYBLUE,
             inactiveColor = COLOR_DARKGRAY;
    const int line = 7;
    int displayWidth = matrix.width();
    matrix.drawFastHLine(0, line, displayWidth, COLOR_BLACK);
    if (totalRounds < displayWidth / 2) {
        int lineWidth = displayWidth / totalRounds, offset;
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
            matrix.drawFastHLine(i * lineWidth + offset, line, lineWidth - 1, (i == currentRound - 1) ? activeColor : inactiveColor);
        }
    } else {
        int t = totalRounds <= displayWidth ? totalRounds : displayWidth;
        int start = (displayWidth - (t + (t <= displayWidth - 2 ? 2 : 0))) / 2;
        for (int i = 0; i < t; i++) {
            if (totalRounds <= displayWidth - 2 && (i == currentRound - 1 || i == currentRound)) {
                start++;
            }
            matrix.drawPixel(start + i, line, (i == currentRound - 1) ? activeColor : inactiveColor);
        }
    }
}

void countdownUpdate() {
    if (!updateCountdown) {
        return;
    }
    updateCountdown = false;

    long remaining = BCPEvent.roundEndEpoch() - time(nullptr);
    bool dontDisplayHours = BCPEvent.timerLength() <= 3600;

    if (BCPEvent.timerPaused()) {
        // timer is paused
        remaining = BCPEvent.pausedTimeRemaining();
        matrix.setTextColor(time(nullptr) % 2 ? COLOR_MAGENTA : COLOR_BLACK);
    } else if (remaining <= Config.redThreshold()) {
        // we are below red threshold of the timer
        matrix.setTextColor(COLOR_RED);
    } else if (remaining <= Config.yellowThreshold()) {
        // we are below yellow threshold of the timer
        matrix.setTextColor(COLOR_YELLOW);
    } else if (remaining > BCPEvent.timerLength() || (remaining == BCPEvent.timerLength() && BCPEvent.timerLength() <= 3600)) {
        // event round not yet started, show time to start
        matrix.setTextColor(COLOR_GREEN);
        remaining -= BCPEvent.timerLength();
        dontDisplayHours = remaining < 3600 && BCPEvent.timerLength() <= 3600;
    } else {
        matrix.setTextColor(COLOR_WHITE);
    }

    String out  = remaining < 0 ? "-" : "";;
    int absoluteTime = remaining < 0 ? -remaining : remaining;
    int hours = absoluteTime / 3600;
    int minutes = (absoluteTime % 3600) / 60;
    int seconds = absoluteTime % 60;

    int x, y;

    if (dontDisplayHours) {
        // mm:ss
        matrix.setFont(mmssFont);
        x = remaining < 0 ? 1 : 5;
        y = 6;
        if (remaining < -3599) {
            out = "-//://";    // To save space in the font, X bitmap mapped to / character
        } else {
            out += String(minutes / 10) + String(minutes % 10) + ":" +
                   String(seconds / 10) + String(seconds % 10);
        }
    } else {
        // h:mm:ss
        matrix.setFont(hmmssFont);
        x = remaining < 0 ? -1 : 3;
        y = 1;
        if (remaining < -35999) {
            out = "-/://://";  // To save space in the font, X bitmap mapped to / character
        } else {
            out += String(hours) + ":" +
                String(minutes / 10) + String(minutes % 10) + ":" +
                String(seconds / 10) + String(seconds % 10);
        }
    }

    matrix.clear();
    matrix.setCursor(x, 6);
    matrix.print(out);
    displayRound(BCPEvent.currentRound(), BCPEvent.numberOfRounds());
    matrix.show();
    matrix.setFont((GFXfont *) defaultFont);
}  

// ---- update display after data refresh ----

void displayUpdate(bool afterScrollOnce = false) {
    MAIN_DEBUG("Updating display, current state: " + String(DISPLAY_STATE));

    if (displayState == DISPLAY_SCROLL_ONCE || displayState == DISPLAY_EVENT_NO_UPDATE) {
        return;
    }

    if (BCPEvent.valid()) {
        MAIN_DEBUG("BCP event is valid.");

        if (BCPEvent.ended() || !BCPEvent.started()) {
            // event either not started or already ended
            static String eventName = "";
            if (displayState != DISPLAY_EVENT_NAME || eventName != BCPEvent.name()) {
                MAIN_DEBUG("Displaying event name.");
                stopCountdownTimer();
                stopAnimations();
                matrix.clear();
                ensureAnimationsTimer();
                scrollingText = new ScrollingText(&matrix, BCPEvent.name(), 0, 7, defaultFont, true);
                matrix.show();
                eventName = BCPEvent.name();
                displayState = DISPLAY_EVENT_NAME;
            }

        } else if (BCPEvent.timerLength() <= 0) {
            // no timer, show round only
            static int lastRound = -1;
            if (displayState != DISPLAY_EVENT_ROUND || lastRound != BCPEvent.currentRound()) {
                MAIN_DEBUG("Displaying event round.");
                stopCountdownTimer();
                stopAnimations();
                matrix.clear();
                matrix.setTextColor(COLOR_WHITE);
                int16_t  x1, y1;
                uint16_t w, h;
                String msg = "Round " + String(BCPEvent.currentRound());
                matrix.getTextBounds(msg, 0, 7, &x1, &y1, &w, &h);
                matrix.setCursor((matrix.width() - w) / 2, 7);
                matrix.print(msg);
                matrix.show();
                lastRound = BCPEvent.currentRound();
                displayState = DISPLAY_EVENT_ROUND;
            }

        } else {
            // show countdown
            if (displayState != DISPLAY_EVENT_COUNTDOWN) {
                stopAnimations();
                ensureCountdownTimer();
                displayState = DISPLAY_EVENT_COUNTDOWN;
            }
        }

    } else if (displayState != DISPLAY_INVALID_EVENT) {
        // invalid event ID, start config mode
        stopCountdownTimer();
        stopAnimations();
        Config.startConfigServer();
        configMessage(true);
        displayState = DISPLAY_INVALID_EVENT;
    }
}

// ---- event handler running on 2nd core ----

void eventHandler(void* parameter) {
    while (true) {
        bool showMatrix;
        showMatrix = false;
        if (midButtonPressed) {
            static unsigned long lastPress = millis();
            unsigned long now = millis();
            if (now - lastPress >= DEBOUNCE_DELAY_MS) {
                lastPress = now;
                midButtonPressed = false;
                stopCountdownTimer();
                stopAnimations();
                Config.startConfigServer();
                configMessage();
                displayState = DISPLAY_SCROLL_ONCE;
            }
        }
        if (Config.isConfigServerRunning()) {
            Config.handleClient();
        }
        if (displayState == DISPLAY_SCROLL_ONCE && scrollingText && !scrollingText->isActive) {
            displayState = DISPLAY_BOOT;
            displayUpdate(true);
        }
        if (updateScrollingText && scrollingText && scrollingText->isActive) {
            updateScrollingText = false;
            scrollingText->step(true);
            showMatrix = true;
        }
        if (updateProgress && progressIndicator && progressIndicator->isActive) {
            updateProgress = false;
            progressIndicator->step(true);
            showMatrix = true;
        }
        if (updateCountdown && displayState == DISPLAY_EVENT_COUNTDOWN) {
            countdownUpdate();
        }
        if (showMatrix) {
            matrix.show();
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
        splashScreen();
        delay(250); // keep the server responsive for 0.25 second, let the http client receive the response
    }
    Config.stopConfigServer(); // no longer needed
}

// ---- core Arduino functions ----

void setup() {
    pinMode(PIN_BUZZER, INPUT_PULLDOWN);  // stop whistle noise
    pinMode(27, INPUT_PULLUP);    // mid button
    Serial.begin(9600);
    while (!Serial);              // wait for serial port to connect. Needed for native USB
    debugOut_ = &Serial;

    FastLED.addLeds<NEOPIXEL,PIN_LED_MATRIX>(matrixleds, 256);
    matrix.begin();
    matrix.setTextWrap(false);
    // matrix.setBrightness(40);
    matrix.setTextColor(matrix.Color(255, 255, 255));
    matrix.setFont((GFXfont *) defaultFont);

    xTaskCreatePinnedToCore(
            eventHandler,     // Function to run
            "EventHandler",   // Task name
            4096*2,           // Stack size
            NULL,             // Parameter
            1,                // Priority
            NULL,             // Task handle
            1                 // Core (0 or 1)
        );
/*
    delay(5000);
    ensureAnimationsTimer();
    scrollingText = new ScrollingText(&matrix, "Přerov...", 0, 7, (const GFXfont *) defaultFont);
    while (true) {
        delay(1000);
    }
*/
    splashScreen();
    WifiMgr.connect(digitalRead(27) == LOW, 0, new WifiMgrMsg((const GFXfont &) *defaultFont), debugOut_);    // if mid button pressed, enforce portal
    Config.debugOut = debugOut_;
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
            stopAnimations();
            splashScreen();
            BCPEvent.setID(Config.eventId());
            BCPEvent.refreshData();
            stopAnimations();
            displayState = DISPLAY_BOOT;
            displayUpdate();
        }
    } else if (count % 120 == 0) {
        MAIN_DEBUG("Refreshing BCP event data...");
        if (BCPEvent.refreshData()) {
            MAIN_DEBUG("BCP event data refreshed successfully");
            failCount = 0;
        } else {
            failCount++;
            MAIN_DEBUG("Failed to refresh BCP event data (failure count " + String(failCount) + ")");
            if (failCount >= 5) {
                MAIN_DEBUG("Too many consecutive failures, restarting device");
                ESP.restart();
            }
        }
        if (count == 0) {
            stopAnimations();
        }
        displayUpdate();
    }
    if (count == 0) {
        attachInterrupt(27, []() { midButtonPressed = true; }, FALLING);
    }
    delay(500);
    count++;    
}