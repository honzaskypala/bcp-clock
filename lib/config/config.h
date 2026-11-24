// Configuration management for BCP-clock
// (c) 2025 Honza Skýpala
// WTFPL license applies

#ifndef CONFIG_H
#define CONFIG_H

#include <buildfeatures.h>
#include <Arduino.h>
#include <WebServer.h>
#include <Preferences.h>

struct ConfigValues {
    String event;
    long yellow, red;   // seconds
    bool beep;          // acoustic signals at thresholds
    String hostname;
    uint8_t mode;       // ModeId of the last mode
#if BCP_MODE_OFFLINE
    long offline;       // seconds, length of the offline countdown
#endif
#if BCP_MODE_CHESS
    long chess;         // seconds, chess clock round (both players)
#endif
};

// Reads the configuration from an opened Preferences namespace, defaults for missing keys
ConfigValues readConfig(Preferences& prefs);

class CConfig {
public:
    // ---- Configuration parameter getters ----
    const String& eventId() const { return event_; }
    long yellowThreshold() const { return yellow_; }
    long redThreshold() const { return red_; }
    bool isAcousticSignalsEnabled() const { return beep_; }
    const String& hostname() const { return hostname_; }
    uint8_t mode() const { return mode_; }  // ModeId of the last mode, also when not in the build
#if BCP_MODE_OFFLINE
    long offlineLength() const { return offline_; }
#endif
#if BCP_MODE_CHESS
    long chessLength() const { return chess_; }  // whole round, each player has half
#endif

    // ---- State saved outside the config page ----
    void setMode(uint8_t mode);             // saved right away
    // State of a mode (plain data) in NVS namespace "state"; load fails if missing or of another size
    bool loadState(const char *key, void *data, size_t size);
    void saveState(const char *key, const void *data, size_t size);

    // ---- Erase configuration from persistent storage ----
    void erase();
    
    // ---- Configuration HTTP server ----
    void startConfigServer(unsigned long timeoutMs = 5UL * 60UL * 1000UL); // default 5 minutes; 0 = no timeout
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
    bool beep_;
    String hostname_;
    uint8_t mode_;
#if BCP_MODE_OFFLINE
    long offline_;
#endif
#if BCP_MODE_CHESS
    long chess_;
#endif
    void savePreferences();
    
    // ---- Configuration HTTP server ----
    WebServer* configServer = nullptr;
    void handleConfigRoot();
    String statusMessage();
    void appendConfigFormHtml(String& html);
    void handleConfigPost();
#if BCP_FEATURE_WIFIMGR
    void handleWifiDelete();
    void handleWifiDeleteAll();
#endif
#if BCP_FEATURE_MDNS
    void startMdns();
#endif

    // ---- HTTP server timeout tracking ----
    unsigned long _serverStartMs;   // millis() when server started/reset
    unsigned long _serverTimeoutMs; // 0 = no timeout

    // ---- Helpers ----
    bool ensureNvsReady();
};

extern CConfig& Config;

#endif