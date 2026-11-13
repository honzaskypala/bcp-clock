// Configuration management for BCP-clock
// (c) 2025 Honza Skýpala
// WTFPL license applies

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <WebServer.h>
#include <Preferences.h>

struct ConfigValues {
    String event;
    long yellow, red;   // seconds
    String hostname;
};

// Reads the configuration from an opened Preferences namespace, defaults for missing keys
ConfigValues readConfig(Preferences& prefs);

class CConfig {
public:
    // ---- Configuration parameter getters ----
    const String& eventId() const { return event_; }
    long yellowThreshold() const { return yellow_; }
    long redThreshold() const { return red_; }
    const String& hostname() const { return hostname_; }

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
    String statusMessage();
    void appendConfigFormHtml(String& html);
    void handleConfigPost();
    void handleWifiDelete();
    void handleWifiDeleteAll();

    // ---- HTTP server timeout tracking ----
    unsigned long _serverStartMs;   // millis() when server started/reset
    unsigned long _serverTimeoutMs; // 0 = no timeout

    // ---- Helpers ----
    bool ensureNvsReady();
};

extern CConfig& Config;

#endif