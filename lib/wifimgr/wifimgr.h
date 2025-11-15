#pragma once

// ESP32 WiFi Manager with captive portal for BCP clock
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include <Arduino.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>

class CWifiMgr {
public:
    bool connect(uint32_t portalTimeoutMs = 0);
    bool isConnected() const { return connected_; }

private:
    // ---- Core steps ----
    bool scanAndSort();
    void freeScanData();
    bool attemptConnect(const char* ssid, const char* pass, uint32_t timeoutMs);
    bool loadCredentialsDoc(ArduinoJson::JsonDocument& doc);
    bool saveCredential(const char* ssid, const char* pass);
    bool removeCredential(const char* ssid);
    void ensureStaModeForScan();

    // ---- Time sync ----
    bool syncTime(uint32_t timeoutMs = 10000);  // CHANGED: now returns success/failure
    void printLocalTime();

    // ---- Portal ----
    void startConfigPortal();
    void stopPortal();

    // ---- HTTP Handlers ----
    void handleRoot();
    void handleRescan();
    void handleConnect();
    void handleDelete();
    void handleCaptive();
    void handleNotFound();

    // ---- Helpers ----
    static bool mountLittleFS();
    static String htmlEscape(const String& in);
    static String urlEncode(const String& in);
    static inline bool isOpenEncryption(int enc) { return enc == WIFI_AUTH_OPEN; }
    static const char* wifiAuthModeToString(uint8_t m);

private:
    DNSServer dnsServer_;
    WebServer server_{80};

    int*     scanIdx_   = nullptr;
    int*     scanRssi_  = nullptr;
    String*  scanSsid_  = nullptr;
    uint8_t* scanEnc_   = nullptr;
    int*     scanChan_  = nullptr;
    int      scanCount_ = 0;

    volatile bool connected_ = false;
    bool portalActive_ = false;

    bool timeSyncFailed_ = false;
};

extern CWifiMgr WifiMgr;