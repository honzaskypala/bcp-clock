// Hardware interface implementation for Ulanzi TC001 desktop clock
// (c) 2025 Honza Skýpala
// WTFPL license applies

#pragma once

#include "hw.h"
#include <Arduino.h>
#include <Print.h>

#include <Adafruit_GFX.h>
#include <FastLED.h>
#include <FastLED_NeoMatrix.h>

#include <progress_indicator.h>
#include <scrolling_text.h>

class Tc001 : public Hw, public Print {

// ---- Singleton pattern ----
public:
    static Tc001& getInstance() {
        static Tc001 instance;
        return instance;
    }
private:
    Tc001();
    Tc001(const Tc001&) = delete;
    Tc001& operator=(const Tc001&) = delete;

// ---- Hw interface and related methods ----
public:
    virtual void showSplashScreen(bool showProgress = true) override;
    virtual bool ensureConnection() override;

    virtual void displayEventName(const CBCPEvent& event) override;
    virtual void displayEventRound(const CBCPEvent& event) override;
    virtual void displayCountdown(const CBCPEvent& event) override;

    static void handleEvents(void * parameter);

    virtual void showConfigServerMsg(const char *msg) override;

    virtual void reboot() override;

#ifdef BCP_DEV
// ---- Development helpers (dev build only) ----
public:
    static void simulateMidButton() { midButtonPressed = true; }
    static void dumpScreen(Print& out);
    static uint32_t countdownDraws;  // number of countdown redraws since boot
    void rebootToPortal();           // reboot and start the captive portal as if Mid was held
#endif

private:
    static CRGB matrixleds[256];
    static FastLED_NeoMatrix matrix;

    static const UTF8_32BitFont *defaultFont;
    static const GFXfont *hmmssFont;
    static const GFXfont *mmssFont;

    static ProgressIndicator *progressIndicator;
    static ScrollingText *scrollingText;
    static unsigned long stMillis, piMillis;
    static time_t countdownDrawnAt;  // time(nullptr) of the last countdown redraw, 0 = redraw now
    static void stopAnimations();

    static bool midButtonPressed;
    static void handleMidButton();
    static bool stepAnimations();

    static const CBCPEvent *countdownEvent;  // event drawn by handleEvents in DISPLAY_EVENT_COUNTDOWN
    static void drawCountdown(const CBCPEvent& event);
    static void displayRoundWithCountdown(int currentRound, int totalRounds);

    static void startProgress();
    static void stepProgress();
    static void stopProgress();

    // loop = true: invalid event ID, message + looping address (DISPLAY_CONFIG_REQUIRED);
    // loop = false: config server started by the user, address scrolled once (DISPLAY_CONFIG_MESSAGE)
    static void showConfigServerMsg(const char *msg, bool loop);

// ---- Print interface ----
public:
    size_t write(uint8_t c) override;

private:
    char buffer[256];
    size_t bufferIndex;
    String initMsg_ = String();
    bool errorShown_ = false;
    void showMessage(const char *msg);
    void showErrorMessage(const char *msg);
    void showInfoMessage(const char *msg);
};