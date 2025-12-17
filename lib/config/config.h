// Configuration management for BCP-clock
// (c) 2025 Honza Skýpala
// WTFPL license applies

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <WebServer.h>

class CConfig {
public:
    // ---- Configuration parameter getters ----
    String eventId() const { return event_; }
    long yellowThreshold() const { return yellow_; }
    long redThreshold() const { return red_; }
    String hostname() const { return hostname_; }

    // ---- Erase configuration from persistent storage ----
    void erase();
    
    // ---- Configuration HTTP server ----
    void startConfigServer(unsigned long timeoutMs = 5UL * 60UL * 1000UL); // default 5 minutes; 0 = no timeout
    void stopConfigServer();
    void handleClient(); // pump HTTP requests
    bool isConfigServerRunning() const { return configServer != nullptr; }
    volatile bool configUpdated = false;

    Print *debugOut = nullptr;  // Optional debug output

private:
    // ---- Singleton related ----
    CConfig();
    CConfig(const CConfig&) = delete;
    CConfig& operator=(const CConfig&) = delete;
    friend CConfig& getConfigInstance();

    // ---- Configuration parameters ----
    String event_;
    long yellow_, red_;
    String hostname_;
    void savePreferences();
    
    // ---- Configuration HTTP server ----
    WebServer* configServer = nullptr;
    void handleConfigRoot();
    void handleConfigPost();
    void handleWifiDelete();
    void handleWifiDeleteAll();
    inline String getCss() const;
    String urlDecode(const String& str) const;
    static String urlEncode(const String& in);

    // ---- HTTP server timeout tracking ----
    unsigned long _serverStartMs;   // millis() when server started/reset
    unsigned long _serverTimeoutMs; // 0 = no timeout
    bool _serverTimedOut;           // kept for compatibility, not used to block restarts

    // ---- Helpers ----
    String secondsToTime(long seconds) const;
    long timeToSeconds(const String& time) const;
    static uint8_t h2int(char c);
    bool ensureNvsReady();
};

extern CConfig& Config;

#endif