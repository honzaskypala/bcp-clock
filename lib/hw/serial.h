#pragma once

#include "hw.h"
#include <WiFi.h>
#include <time.h>

class Serial_hw : public Hw {
public:
    Serial_hw(Print *debugOut = nullptr) : debugOut_(debugOut) {
        Serial.begin(9600);
        while (!Serial);
    }

    virtual void splashScreen(bool showProgress = true) override {
        Serial.println("BCP clock");
        if (showProgress) {
            progressStart();
        }
    }

    virtual bool ensureConnection() override {
        const char* ssid     = "ssid";
        const char* password = "password";
        const unsigned long WIFI_TIMEOUT = 15000;
        debugPrintln("Connecting to WiFi SSID: " + String(ssid));
        progressStart();
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid, password);
        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED) {
            if (millis() - start > WIFI_TIMEOUT) {
                progressStop();
                debugPrintln("Failed to connect within timeout.");
                debugPrintln("WiFi status: " + String(WiFi.status()));
                return false;
            }
            progressStep();
            delay(500);
        }
        progressStop();
        debugPrintln("Connected to WiFi. IP address: " + WiFi.localIP().toString());
        debugPrintln("Syncing time via NTP...");
        static const char* NTP_SERVER = "pool.ntp.org";
        static const long GMT_OFFSET_SEC = 0;
        static const int DAYLIGHT_OFFSET_SEC = 3600;
        static const uint32_t timeoutMs = 10000;
        configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
        struct tm timeinfo;
        if (getLocalTime(&timeinfo, timeoutMs)) {
            debugPrintln("Time synchronized.");
            return true;
        }
        debugPrintln("Failed to obtain time.");
        return false;
    }

    virtual void displayEventName(const CBCPEvent& event) override {
        Serial.println(event.name());
    }

    virtual void displayEventRound(const CBCPEvent& event) override {
        Serial.println("Round " + String(event.currentRound()));
    }

    static void displayCountdown(const CBCPEvent& event) {
        long remaining = event.roundEndEpoch() - time(nullptr);
        String out  = remaining < 0 ? "-" : "";
        int absoluteTime = remaining < 0 ? -remaining : remaining;
        int hours = absoluteTime / 3600;
        int minutes = (absoluteTime % 3600) / 60;
        int seconds = absoluteTime % 60;
        bool dontDisplayHours = event.timerLength() <= 3600 && absoluteTime < 3600;
        if (dontDisplayHours) {
            out += String(minutes / 10) + String(minutes % 10) + ":" +
                   String(seconds / 10) + String(seconds % 10);
        } else {
            out += String(hours / 10) + String(hours % 10) + ":" +
                   String(minutes / 10) + String(minutes % 10) + ":" +
                   String(seconds / 10) + String(seconds % 10);
        }
        Serial.println("Time remaining: " + String(out));
    }

    void progressStart() {
        debugPrint("  ");
    }

    void progressStep() {
        debugWrite('.');
    }

    void progressStop() {
        debugPrintln("");
    }

    virtual void tick() override {
        static unsigned long lMillis = 0;
        if (displayState == DISPLAY_EVENT_COUNTDOWN && (lMillis == 0 || millis() - lMillis > 1000)) {
            lMillis = (lMillis == 0) ? millis() : lMillis + 1000;
            displayCountdown(BCPEvent);
        }
    }

    virtual void configServerMsg(const char *msg) override {
        Serial.println(msg);
    }

    virtual void reboot() override {
#ifdef ARDUINO_ARCH_ESP32
        ESP.restart();
#else
        asm volatile ("jmp 0"); // AVR reset
#endif
    }

private:
    Print *debugOut_ = nullptr;
    void debugPrint(const String& msg) {
        if (debugOut_ != nullptr) {
            debugOut_->print("[Serial_hw] " + msg);
        }
    }

    void debugWrite(const char c) {
        if (debugOut_ != nullptr) {
            debugOut_->write(c);
        }
    }

    void debugPrintln(const String& msg) {
        if (debugOut_ != nullptr) {
            debugPrint(msg);
            debugOut_->println();
        }
    }
};