// Configuration management for BCP-clock
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include "config.h"
#include <Preferences.h>
#include <nvs_flash.h>
#include <wifimgr.h>
#include <webui.h>
#include <textutil.h>

#define CONFIG_DEBUG(msg) if (debugOut) { debugOut->print("[Config] "); debugOut->println(msg); }

static const String DEF_EVENT = "";
static const long DEF_YELLOW = 600; // seconds
static const long DEF_RED    = 0;   // seconds
static const String DEF_HOSTNAME = "BCP-clock";
static constexpr const char* CONFIG_NS = "config";
// ---- Read configuration values from an opened Preferences namespace ----

ConfigValues readConfig(Preferences& prefs) {
    ConfigValues v;
    v.event = prefs.getString("event", DEF_EVENT);
    // Prefer integer keys; if missing or type-mismatch, try string fallback
    v.yellow = prefs.getLong("yellow", DEF_YELLOW);
    v.red = prefs.getLong("red", DEF_RED);
    String yStr = prefs.getString("yellow", "");
    String rStr = prefs.getString("red", "");
    if (yStr.length() > 0) {
        // parse m[:ss] as minutes/seconds (same as config webpage form)
        v.yellow = timeToSeconds(yStr);
    }
    if (rStr.length() > 0) {
        v.red = timeToSeconds(rStr);
    }
    v.hostname = sanitizeHostname(prefs.getString("hostname", DEF_HOSTNAME));
    return v;
}

// ---- Constructor: load configuration from persistent storage ----

CConfig::CConfig() {
    // Default configuration values
    event_ = DEF_EVENT;
    yellow_ = DEF_YELLOW;
    red_ = DEF_RED;
    hostname_ = DEF_HOSTNAME;

    // Load configuration from persistent storage if available
    if (!ensureNvsReady()) {
        // NVS not ready, keep defaults
        return;
    }
    Preferences prefs;
    if (prefs.begin(CONFIG_NS, true)) { // read-only
        ConfigValues v = readConfig(prefs);
        event_    = v.event;
        yellow_   = v.yellow;
        red_      = v.red;
        hostname_ = v.hostname;
        prefs.end();
        CONFIG_DEBUG("Preferences loaded");
    } else {
        CONFIG_DEBUG("Failed to open preferences for reading");
    }
}

// ---- Save configuration to persistent storage ----

void CConfig::savePreferences() {
    if (!ensureNvsReady()) {
        CONFIG_DEBUG("NVS not ready for writing");
        return;
    }
    Preferences prefs;
    if (prefs.begin(CONFIG_NS, false)) { // write mode
        CONFIG_DEBUG("Saving preferences");
        prefs.putString("event", event_);
        prefs.putLong("yellow", yellow_);
        prefs.putLong("red", red_);
        prefs.putString("hostname", hostname_);
        prefs.end();
        CONFIG_DEBUG("Preferences saved");
    } else {
        CONFIG_DEBUG("Failed to open preferences for writing");
    }
}

// ---- Erase configuration from persistent storage ----

void CConfig::erase() {
    if (!ensureNvsReady()) {
        CONFIG_DEBUG("NVS not ready for clearing");
        return;
    }
    Preferences prefs;
    if (prefs.begin(CONFIG_NS, false)) { // write mode
        CONFIG_DEBUG("Clearing preferences");
        prefs.clear();
        prefs.end();
        CONFIG_DEBUG("Preferences cleared");
        // Reset to defaults
        event_ = DEF_EVENT;
        yellow_ = DEF_YELLOW;
        red_ = DEF_RED;
        hostname_ = DEF_HOSTNAME;
    } else {
        CONFIG_DEBUG("Failed to open preferences for clearing");
    }
}

// ---- Configuration HTTP server ----

void CConfig::startConfigServer(unsigned long timeoutMs) {
    // If already running, just update timeout and reset the start time
    if (configServer != nullptr) {
        _serverTimeoutMs = timeoutMs;      // 0 => no timeout
        _serverStartMs = millis();         // reset timer window
        return;
    }
    configServer = new WebServer(80);
    
    configServer->on("/", HTTP_GET, [this]() { handleConfigRoot(); });
    configServer->on("/config", HTTP_POST, [this]() { handleConfigPost(); });
    // same paths as the captive portal, the stored networks HTML is shared (WifiMgr.storedWifisHTML())
    configServer->on("/delete", HTTP_DELETE, [this]() { handleWifiDelete(); });
    configServer->on("/deleteAll", HTTP_DELETE, [this]() { handleWifiDeleteAll(); });

    // Handle accidental GET on /config by redirecting to root
    configServer->on("/config", HTTP_GET, [this]() {
        configServer->sendHeader("Location", "/");
        configServer->send(302);
    });

    // Avoid error log from browser asking for favicon
    configServer->on("/favicon.ico", HTTP_GET, [this]() {
        WebServer* srv = configServer;
        if (srv) srv->send(204);
    });

    // Catch-all for unknown paths
    configServer->onNotFound([this]() {
        configServer->send(404, "text/plain", "Not found");
    });
    
    configServer->begin();
    _serverStartMs = millis();
    _serverTimeoutMs = timeoutMs; // 0 => no timeout
    CONFIG_DEBUG("HTTP server started on http://" + WiFi.localIP().toString() + (timeoutMs == 0 ? " (no timeout)" : " (with timeout)"));
}

void CConfig::stopConfigServer() {
    if (configServer != nullptr) {
        configServer->stop();
        delete configServer;
        configServer = nullptr;
        CONFIG_DEBUG("HTTP server stopped");
    }
}

void CConfig::handleClient() {
    if (configServer != nullptr) {
        configServer->handleClient();
        // Stop server after timeout, if configured
        if (_serverTimeoutMs > 0 && (millis() - _serverStartMs) >= _serverTimeoutMs) {
            CONFIG_DEBUG("HTTP server timeout reached, stopping");
            stopConfigServer();
        }
    }
}

void CConfig::handleConfigRoot() {
    String status = "";
    if (configServer->hasArg("saved")) {
        status = "Configuration saved";
    }
    if (configServer->hasArg("wifiDeleted")) {
        status = "WiFi network deleted";
    }
    if (configServer->hasArg("wifiAllDeleted")) {
        status = "All WiFi networks deleted";
    }
    
    String page = WebUI::pageStart("BCP-clock config",
            "label[for=yellow],label[for=red]{display:flex;align-items:center;background:#000;padding:0 8px;box-sizing:border-box}"
            ".row.threshold{align-items:stretch}"
            "label[for=yellow]{color:#ff0}"
            "label[for=red]{color:red}"
            ".row.threshold{margin:4px 0}"
            ".input-wrapper{position:relative;flex:1 1 auto}"
            ".clear-text{display:none}"
            "@media (pointer:coarse){"
                ".input-wrapper input{padding-right:2.5rem}"
                ".clear-text{display:block;position:absolute;right:.7rem;top:50%;transform:translateY(-50%);text-decoration:none;cursor:pointer;color:grey}"
            "}");
    page.reserve(9000);

    if (status.length() > 0) {
        page += "<div class='status ok'>" + status + "</div>";
    }

    page += "<form method='POST' action='/config'>"
                "<div class='row'>"
                    "<label for='event'>Event:</label>"
                    "<div class='input-wrapper'>"
                        "<input type='text' id='event' name='event' placeholder='Enter event ID or event URL' value='" + WebUI::htmlEscape(event_) + "'>"
                        "<a class='clear-text' onclick='clearInput(this);return false;'>⨂</a>"
                    "</div>"
                "</div>";
    page += WebUI::sectionHeader("Colour thresholds");
    page += "<div class='row threshold'>"
                "<label for='yellow'>Yellow:</label>"
                "<input type='text' id='yellow' name='yellow' value='" + secondsToTime(yellow_) + "'>"
            "</div>"
            "<div class='row threshold'>"
                "<label for='red'>Red:</label>"
                "<input type='text' id='red' name='red' value='" + secondsToTime(red_) + "'>"
            "</div>";
    page += WebUI::sectionHeader("Network");
    page += "<div class='row'>"
                "<label for='hostname'>Hostname:</label>"
                "<input type='text' id='hostname' name='hostname' maxlength='32' value='" + WebUI::htmlEscape(hostname_) + "'>"
            "</div>"
            "<div class='actions'><button class='btn btnMain' type='submit'>Save</button></div>"
            "</form>"
            "<script>"
                "function clearInput(el) {"
                    "const wrapper=el.closest('.input-wrapper');"
                    "const input=wrapper.querySelector('input');"
                    "input.value='';"
                    "input.focus();"
                "}"
            "</script>";

    page += WifiMgr.storedWifisHTML();
    page += WebUI::pageEnd();

    configServer->send(200, "text/html", page);
}

void CConfig::handleConfigPost() {
    if (configServer->hasArg("event")) {
        event_ = configServer->arg("event");
    }
    if (configServer->hasArg("yellow")) {
        yellow_ = timeToSeconds(configServer->arg("yellow"));
    }
    if (configServer->hasArg("red")) {
        red_ = timeToSeconds(configServer->arg("red"));
    }
    if (configServer->hasArg("hostname")) {
        hostname_ = sanitizeHostname(configServer->arg("hostname"));
    }
    
    savePreferences();
    
    configServer->sendHeader("Location", "/?saved=1");
    configServer->send(303);

    configUpdated = true;
}

void CConfig::handleWifiDelete() {
    if (!configServer->hasArg("ssid")) {
        configServer->send(400, "text/plain", "Missing ssid parameter");
        return;
    }
    String ssid = configServer->arg("ssid");
    CONFIG_DEBUG("Request to delete WiFi network '" + ssid + "'");

    if (WifiMgr.removeStoredNetwork(ssid.c_str())) {
        configServer->send(200, "text/plain", "Deleted");
    } else {
        configServer->send(404, "text/plain", "Delete failed or network not found");
    }
}

void CConfig::handleWifiDeleteAll() {
    CONFIG_DEBUG("Request to delete ALL stored WiFi networks");
    if (WifiMgr.eraseStoredNetworks()) {
        configServer->send(200, "text/plain", "All deleted");
    } else {
        configServer->send(500, "text/plain", "Delete all failed");
    }
}

// ---- Helpers ----

// Ensure NVS is initialized before using Preferences
bool CConfig::ensureNvsReady() {
    static bool ready = false;
    if (ready) return true;
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    ready = (err == ESP_OK);
    if (!ready && Serial) {
        CONFIG_DEBUG("NVS init failed");
    }
    return ready;
}

// ---- Singleton related ----

CConfig& getConfigInstance() {
    static CConfig instance;
    return instance;
}

CConfig& Config = getConfigInstance();