// Hardware interface implementation for Ulanzi TC001 desktop clock
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include "tc001.h"

#include <f3x5.h>
#include <f4x6.h>
#include <wifimgr.h>
#include <config.h>
#include <bcpevent.h>

#define PIN_BUZZER          15
#define PIN_MID_BUTTON      27
#define PIN_LED_MATRIX      32

#define COLOR_WHITE matrix.Color24to16(CRGB::White)
#define COLOR_BLACK matrix.Color24to16(CRGB::Black)
#define COLOR_RED matrix.Color24to16(CRGB::Red)
#define COLOR_YELLOW matrix.Color24to16(CRGB::Yellow)
#define COLOR_GREEN matrix.Color24to16(CRGB::Green)
#define COLOR_MAGENTA matrix.Color24to16(CRGB::Magenta)
#define COLOR_DEEPSKYBLUE matrix.Color24to16(CRGB::DeepSkyBlue)
#define COLOR_DARKGRAY matrix.Color(20, 20, 20)

#define SCROLLING_TEXT_SPEED 100 // miliseconds per step
#define PROGRESS_INDICATOR_SPEED 100 // milliseconds per step

constexpr int DEBOUNCE_DELAY_MS = 500;

volatile DisplayState Hw::displayState = DISPLAY_BOOT;

CRGB Tc001::matrixleds[256];
FastLED_NeoMatrix Tc001::matrix = FastLED_NeoMatrix(Tc001::matrixleds, 32, 8, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG);

const UTF8_32BitFont* Tc001::defaultFont = &F3x5[0];
const GFXfont* Tc001::hmmssFont = &F3x5_Fixed;
const GFXfont* Tc001::mmssFont = &F4x6;

unsigned long Tc001::stMillis = 0;
unsigned long Tc001::piMillis = 0;
unsigned long Tc001::lMillis = 0;

ProgressIndicator* Tc001::progressIndicator = nullptr;
ScrollingText* Tc001::scrollingText = nullptr;
bool Tc001::midButtonPressed = false;

Tc001::Tc001(Print *debugOut) : debugOut_(debugOut) {
    pinMode(PIN_BUZZER, INPUT_PULLDOWN);      // stop whistle noise
    pinMode(PIN_MID_BUTTON, INPUT_PULLUP);    // mid button
    Serial.begin(9600);
    while (!Serial);

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
}

void Tc001::stopAnimations() {
    if (scrollingText && scrollingText->isActive) {
        scrollingText->isActive = false;
        delete scrollingText;
        scrollingText = nullptr;
    }
    progressStop();
    delay(20); // ensure any ongoing matrix updates are finished
    stMillis = 0;
    piMillis = 0;
    lMillis = 0;
}

void Tc001::splashScreen(bool showProgress) {
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
        progressStart();
    }
    matrix.show();
}

void Tc001::configServerMsg(const char *msg, bool loop) {
    stopAnimations();
    matrix.clear();
    String msgStr = "Config page http://" + WiFi.localIP().toString();
    scrollingText = new ScrollingText(&matrix, msgStr.c_str(), 0, 7, (const GFXfont *) defaultFont, false, COLOR_WHITE, 24, 8, loop);
    matrix.show();
}

void Tc001::configServerMsg(const char *msg) {
    configServerMsg(msg, true);
}

void Tc001::displayRoundWithCountdown(int currentRound, int totalRounds) {
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

void Tc001::displayEventName(const CBCPEvent& event) {
    displayState = DISPLAY_EVENT_NAME;
    stopAnimations();
    matrix.clear();
    scrollingText = new ScrollingText(&matrix, BCPEvent.name(), 0, 7, defaultFont, true);
    matrix.show();
}

void Tc001::displayEventRound(const CBCPEvent& event) {
    displayState = DISPLAY_EVENT_ROUND;
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
}

void Tc001::displayCountdown(const CBCPEvent& event) {
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

    String out  = remaining < 0 ? "-" : "";
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
    displayRoundWithCountdown(BCPEvent.currentRound(), BCPEvent.numberOfRounds());
    matrix.show();
    matrix.setFont((GFXfont *) defaultFont);
}

void Tc001::progressStart() {
    if (progressIndicator == nullptr) {
        progressIndicator = new ProgressIndicator(&matrix, -1, matrix.height() - 1, matrix.width(), COLOR_WHITE, COLOR_BLACK);
    }
    progressIndicator->isActive = true;
}

void Tc001::progressStep() {
    if (progressIndicator != nullptr && progressIndicator->isActive) {
        progressIndicator->step(true);
    }
}

void Tc001::progressStop() {
    if (progressIndicator != nullptr && progressIndicator->isActive) {
        progressIndicator->hide();
        progressIndicator->isActive = false;
    }
}

void Tc001::reboot() {
    ESP.restart();
}

bool Tc001::ensureConnection() {
    bool retValue = WifiMgr.connect(digitalRead(27) == LOW, 0, this, debugOut_);    // if mid button pressed, enforce portal
    if (retValue) {
        attachInterrupt(27, []() { midButtonPressed = true; }, FALLING);
    }
    return retValue;
}

void Tc001::eventHandler(void * parameter) {
    while (true) {
        bool showMatrix;
        showMatrix = false;

        if (midButtonPressed) {
            static unsigned long lastPress = millis();
            unsigned long now = millis();
            if (now - lastPress >= DEBOUNCE_DELAY_MS) {
                lastPress = now;
                midButtonPressed = false;
                displayState = DISPLAY_DONT_UPDATE;
                stopAnimations();
                Config.startConfigServer();
                configServerMsg("", false);
            }
        }

        if (Config.isConfigServerRunning()) {
            Config.handleClient();
        }

        if (displayState == DISPLAY_DONT_UPDATE && scrollingText && !scrollingText->isActive) {
            delete scrollingText;
            scrollingText = nullptr;
            displayState = DISPLAY_ENFORCE_REFRESH;
        }

        if (scrollingText && scrollingText->isActive && (stMillis == 0 || millis() - stMillis > SCROLLING_TEXT_SPEED)) {
            stMillis = (stMillis == 0) ? millis() : stMillis + SCROLLING_TEXT_SPEED;
            scrollingText->step(true);
            showMatrix = true;
        }

        if (progressIndicator && progressIndicator->isActive && (piMillis == 0 || millis() - piMillis > PROGRESS_INDICATOR_SPEED)) {
            piMillis = (piMillis == 0) ? millis() : piMillis + PROGRESS_INDICATOR_SPEED;
            progressStep();
            showMatrix = true;
        }

        if (displayState == DISPLAY_EVENT_COUNTDOWN && (lMillis == 0 || millis() - lMillis > 1000)) {
            lMillis = (lMillis == 0) ? millis() : lMillis + 1000;
            stopAnimations();
            displayCountdown(BCPEvent);
            showMatrix = true;
        }

        if (showMatrix) {
            matrix.show();
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

size_t Tc001::write(uint8_t c) {
    if (c == '\n' || bufferIndex >= sizeof(buffer) - 1) {
        buffer[bufferIndex] = '\0';

        if (strncmp(buffer, "Error:", 6) == 0) {
            // Error message, display in red
            stopAnimations();
            matrix.clear();
            scrollingText = new ScrollingText(&matrix, buffer, 0, 7, (const GFXfont *) defaultFont, false, COLOR_RED);
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
            progressStart();

        } else if (strncmp(buffer, "Connected ", 10) == 0) {
            // do nothing, let the current status message continue

        } else {
            if (errorShown_) {
                scrollingText->append(buffer, COLOR_WHITE, true);
                errorShown_ = false;
            } else {
                stopAnimations();
                matrix.clear();
                scrollingText = new ScrollingText(&matrix, buffer, 0, 7, (const GFXfont *) defaultFont);
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