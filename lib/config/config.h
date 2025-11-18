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
    String getEventId() const { return event_; }
    long getYellowThreshold() const { return yellow_; }
    long getRedThreshold() const { return red_; }
    String getHostname() const { return hostname_; }

    // ---- Erase configuration from persistent storage ----
    void erase();
    
    // ---- Configuration HTTP server ----
    void startConfigServer(bool atStartup = false, unsigned long timeoutMs = 5UL * 60UL * 1000UL); // default 5 minutes; 0 = no timeout
    void stopConfigServer();
    void handleClient(); // pump HTTP requests
    bool isConfigServerRunning() const { return configServer != nullptr; }
    volatile bool configUpdated = false;

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
    bool atStartup_ = false;
    void handleConfigRoot();
    void handleConfigPost();
    inline String getCss() const;
    String urlDecode(const String& str) const;

    // ---- HTTP server timeout tracking ----
    unsigned long _serverStartMs;   // millis() when server started/reset
    unsigned long _serverTimeoutMs; // 0 = no timeout
    bool _serverTimedOut;           // kept for compatibility, not used to block restarts

    // ---- Helpers ----
    String secondsToTime(long seconds) const;
    long timeToSeconds(const String& time) const;
    static uint8_t h2int(char c);
    static bool ensureNvsReady();
};

extern CConfig& Config;

#endif