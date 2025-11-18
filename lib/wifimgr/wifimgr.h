// ESP32 WiFi Manager with captive portal for BCP clock
// (c) 2025 Honza Skýpala
// WTFPL license applies

#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <vector>

class CWifiMgr {
public:
    // ---- Public API ----
    bool connect(uint32_t portalTimeoutMs = 0);
    bool isConnected() const { return connected_; }

private:
    // ---- Core state ----
    volatile bool connected_ = false;
    bool timeSyncFailed_ = false;

    // ---- Core steps ----
    bool scanAndSort();
    void freeScanData();
    bool attemptConnect(const char* ssid, const char* pass, uint32_t timeoutMs);

    // ---- Credentials storage ----
    bool saveCredential(const char* ssid, const char* pass);
    bool removeCredential(const char* ssid);
    bool getCredential(const char* ssid, String& outPass);
    bool listStoredSsids(std::vector<String>& out);
    bool beginPrefs(bool readOnly);
    static String makePassKey(const String& ssid);
    static void splitList(const String& src, std::vector<String>& out);
    static String joinList(const std::vector<String>& list);

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
    static String htmlEscape(const String& in);
    static String urlEncode(const String& in);
    static inline bool isOpenEncryption(int enc) { return enc == WIFI_AUTH_OPEN; }
    static const char* wifiAuthModeToString(uint8_t m);

    // ---- Preferences storage ----
    Preferences prefs_;

    // ---- DNS and HTTP servers for captive portal ----
    DNSServer dnsServer_;
    WebServer server_{80};
    bool portalActive_ = false;

    // ---- Wi-Fi scan results ----
    int*     scanIdx_   = nullptr;
    int*     scanRssi_  = nullptr;
    String*  scanSsid_  = nullptr;
    uint8_t* scanEnc_   = nullptr;
    int*     scanChan_  = nullptr;
    int      scanCount_ = 0;
};

// ---- Singleton instance ----
extern CWifiMgr WifiMgr;