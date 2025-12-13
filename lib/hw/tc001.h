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
    static Tc001& getInstance(Print *debugOut = nullptr) {
        static Tc001 instance(debugOut);
        return instance;
    }
private:
    Tc001(Print *debugOut = nullptr);
    Tc001(const Tc001&) = delete;
    Tc001& operator=(const Tc001&) = delete;

// ---- Hw interface and related methods ----
public:
    virtual void splashScreen(bool showProgress = true) override;
    virtual bool ensureConnection() override;

    virtual void displayEventName(const CBCPEvent& event) override;
    virtual void displayEventRound(const CBCPEvent& event) override;

    static void eventHandler(void * parameter);

    virtual void configServerMsg(const char *msg) override;

    virtual void reboot() override;

private:
    Print *debugOut_ = nullptr;
    void debugPrintln(const String& msg) {
        if (debugOut_ != nullptr) {
            debugOut_->println("[Tc001] " + msg);
        }
    }

    static CRGB matrixleds[256];
    static FastLED_NeoMatrix matrix;

    static const UTF8_32BitFont *defaultFont;
    static const GFXfont *hmmssFont;
    static const GFXfont *mmssFont;

    static ProgressIndicator *progressIndicator;
    static ScrollingText *scrollingText;
    static unsigned long stMillis, piMillis, lMillis;
    static void stopAnimations();

    static bool midButtonPressed;

    static void displayCountdown(const CBCPEvent& event);
    static void displayRoundWithCountdown(int currentRound, int totalRounds);

    static void progressStart();
    static void progressStep();
    static void progressStop();

    static void configServerMsg(const char *msg, bool loop = true);

// ---- Print interface ----
public:
    size_t write(uint8_t c) override;

private:
    char buffer[256];
    size_t bufferIndex;
    String initMsg_ = String();
    bool errorShown_ = false;
};