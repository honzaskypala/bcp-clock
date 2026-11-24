// Configuration management for BCP-clock
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include "config.h"
#include <Preferences.h>
#if BCP_FEATURE_MDNS
#include <ESPmDNS.h>
#endif
#include <nvs_flash.h>
#if BCP_FEATURE_WIFIMGR
#include <wifimgr.h>
#endif
#include <webui.h>
#include <textutil.h>
#if BCP_FEATURE_BRIGHTNESS
#include <brightness.h>
#endif
#include <devlog.h>

#define CONFIG_DEBUG(msg) DEV_LOG("Config", msg)

// Firmware version, set by the release build (PLATFORMIO_BUILD_FLAGS), "dev" for local builds
#ifndef BCP_VERSION
#define BCP_VERSION "dev"
#endif

static const String DEF_EVENT = "";
static const long DEF_YELLOW = 600; // seconds
static const long DEF_RED    = 0;   // seconds
static const bool DEF_BEEP   = true;
static const String DEF_HOSTNAME = "BCP-clock";
static const uint8_t DEF_MODE = 0;   // MODE_BCP, the first mode of the build if not in it
#if BCP_MODE_OFFLINE
static const long DEF_OFFLINE = 3600; // seconds
#endif
#if BCP_MODE_CHESS
static const long DEF_CHESS = 3600;   // seconds, both players
#endif
static constexpr const char* STATE_NS = "state";
#if BCP_FEATURE_BRIGHTNESS
static const uint8_t DEF_BRIGHT = BRIGHTNESS_MAX;   // percent
#endif
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
    v.beep = prefs.getBool("beep", DEF_BEEP);
    v.hostname = sanitizeHostname(prefs.getString("hostname", DEF_HOSTNAME));
    v.mode = prefs.getUChar("mode", DEF_MODE);
#if BCP_MODE_OFFLINE
    v.offline = prefs.getLong("offline", DEF_OFFLINE);
#endif
#if BCP_MODE_CHESS
    v.chess = prefs.getLong("chess", DEF_CHESS);
#endif
#if BCP_FEATURE_BRIGHTNESS
    v.brightness = normalizeBrightness(prefs.getUChar("bright", DEF_BRIGHT));
#endif
    return v;
}

// ---- Constructor: load configuration from persistent storage ----

CConfig::CConfig() {
    serverMutex_ = xSemaphoreCreateRecursiveMutex();

    // Default configuration values
    event_ = DEF_EVENT;
    yellow_ = DEF_YELLOW;
    red_ = DEF_RED;
    beep_ = DEF_BEEP;
    hostname_ = DEF_HOSTNAME;
    mode_ = DEF_MODE;
#if BCP_MODE_OFFLINE
    offline_ = DEF_OFFLINE;
#endif
#if BCP_MODE_CHESS
    chess_ = DEF_CHESS;
#endif
#if BCP_FEATURE_BRIGHTNESS
    brightness_ = DEF_BRIGHT;
#endif

    // Load configuration from persistent storage if available
    if (!ensureNvsReady()) {
        // NVS not ready, keep defaults
        return;
    }
    Preferences prefs;
    if (!prefs.begin(CONFIG_NS, true)) { // read-only
        CONFIG_DEBUG("Failed to open preferences for reading");
        return;
    }
    ConfigValues v = readConfig(prefs);
    event_    = v.event;
    yellow_   = v.yellow;
    red_      = v.red;
    beep_     = v.beep;
    hostname_ = v.hostname;
    mode_     = v.mode;
#if BCP_MODE_OFFLINE
    offline_  = v.offline;
#endif
#if BCP_MODE_CHESS
    chess_    = v.chess;
#endif
#if BCP_FEATURE_BRIGHTNESS
    brightness_ = v.brightness;
#endif
    prefs.end();
    CONFIG_DEBUG("Preferences loaded");
}

// ---- Save configuration to persistent storage ----

void CConfig::savePreferences() {
    if (!ensureNvsReady()) {
        CONFIG_DEBUG("NVS not ready for writing");
        return;
    }
    Preferences prefs;
    if (!prefs.begin(CONFIG_NS, false)) { // write mode
        CONFIG_DEBUG("Failed to open preferences for writing");
        return;
    }
    CONFIG_DEBUG("Saving preferences");
    prefs.putString("event", event_);
    prefs.putLong("yellow", yellow_);
    prefs.putLong("red", red_);
    prefs.putBool("beep", beep_);
    prefs.putString("hostname", hostname_);
#if BCP_MODE_OFFLINE
    prefs.putLong("offline", offline_);
#endif
#if BCP_MODE_CHESS
    prefs.putLong("chess", chess_);
#endif
#if BCP_FEATURE_BRIGHTNESS
    prefs.putUChar("bright", brightness_);
#endif
    prefs.end();
    CONFIG_DEBUG("Preferences saved");
}

void CConfig::setMode(uint8_t mode) {
    if (mode == mode_) return;
    mode_ = mode;
    if (!ensureNvsReady()) return;
    Preferences prefs;
    if (!prefs.begin(CONFIG_NS, false)) {
        CONFIG_DEBUG("Failed to open preferences for writing");
        return;
    }
    prefs.putUChar("mode", mode_);
    prefs.end();
}

bool CConfig::loadState(const char *key, void *data, size_t size) {
    if (!ensureNvsReady()) return false;
    Preferences prefs;
    if (!prefs.begin(STATE_NS, true)) return false;  // fails until the namespace exists
    bool ok = prefs.getBytesLength(key) == size && prefs.getBytes(key, data, size) == size;
    prefs.end();
    return ok;
}

void CConfig::saveState(const char *key, const void *data, size_t size) {
    if (!ensureNvsReady()) return;
    Preferences prefs;
    if (!prefs.begin(STATE_NS, false)) {
        CONFIG_DEBUG("Failed to open state for writing");
        return;
    }
    prefs.putBytes(key, data, size);
    prefs.end();
}

// ---- Erase configuration from persistent storage ----

void CConfig::erase() {
    if (!ensureNvsReady()) {
        CONFIG_DEBUG("NVS not ready for clearing");
        return;
    }
    Preferences prefs;
    if (!prefs.begin(CONFIG_NS, false)) { // write mode
        CONFIG_DEBUG("Failed to open preferences for clearing");
        return;
    }
    CONFIG_DEBUG("Clearing preferences");
    prefs.clear();
    prefs.end();
    CONFIG_DEBUG("Preferences cleared");
    // Reset to defaults
    event_ = DEF_EVENT;
    yellow_ = DEF_YELLOW;
    red_ = DEF_RED;
    beep_ = DEF_BEEP;
    hostname_ = DEF_HOSTNAME;
    mode_ = DEF_MODE;
#if BCP_MODE_OFFLINE
    offline_ = DEF_OFFLINE;
#endif
#if BCP_MODE_CHESS
    chess_ = DEF_CHESS;
#endif
#if BCP_FEATURE_BRIGHTNESS
    brightness_ = DEF_BRIGHT;
#endif
}

// ---- Configuration HTTP server ----

// Holds the server mutex for the scope: the event task must not see a half started or stopped server
class ServerLock {
public:
    explicit ServerLock(SemaphoreHandle_t mutex) : mutex_(mutex) { xSemaphoreTakeRecursive(mutex_, portMAX_DELAY); }
    ~ServerLock() { xSemaphoreGiveRecursive(mutex_); }
    ServerLock(const ServerLock&) = delete;
    ServerLock& operator=(const ServerLock&) = delete;
private:
    SemaphoreHandle_t mutex_;
};

void CConfig::startConfigServer(unsigned long timeoutMs) {
    ServerLock lock(serverMutex_);
    // If already running, just update timeout and reset the start time
    if (configServer != nullptr) {
        _serverTimeoutMs = timeoutMs;      // 0 => no timeout
        _serverStartMs = millis();         // reset timer window
        return;
    }
    configServer = new WebServer(80);
    
    configServer->on("/", HTTP_GET, [this]() { handleConfigRoot(); });
    configServer->on("/config", HTTP_POST, [this]() { handleConfigPost(); });
#if BCP_FEATURE_WIFIMGR
    // same paths as the captive portal, the stored networks HTML is shared (WifiMgr.buildStoredWifisHtml())
    configServer->on("/delete", HTTP_DELETE, [this]() { handleWifiDelete(); });
    configServer->on("/deleteAll", HTTP_DELETE, [this]() { handleWifiDeleteAll(); });
#endif

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
#if BCP_FEATURE_MDNS
    startMdns();
#endif
    _serverStartMs = millis();
    _serverTimeoutMs = timeoutMs; // 0 => no timeout
    CONFIG_DEBUG("HTTP server started on http://" + WiFi.localIP().toString() + (timeoutMs == 0 ? " (no timeout)" : " (with timeout)"));
}

void CConfig::stopConfigServer() {
    ServerLock lock(serverMutex_);
    if (configServer == nullptr) return;
#if BCP_FEATURE_MDNS
    MDNS.end();
#endif
    configServer->stop();
    delete configServer;
    configServer = nullptr;
    CONFIG_DEBUG("HTTP server stopped");
}

#if BCP_FEATURE_MDNS
// The config page is also reachable at http://<hostname>.local while the server runs
void CConfig::startMdns() {
    if (MDNS.begin(hostname_.c_str())) {
        CONFIG_DEBUG("mDNS started: http://" + hostname_ + ".local");
    } else {
        CONFIG_DEBUG("mDNS start failed");
    }
}
#endif

void CConfig::handleClient() {
    ServerLock lock(serverMutex_);
    if (configServer == nullptr) return;
    configServer->handleClient();
    // Stop server after timeout, if configured
    if (_serverTimeoutMs > 0 && (millis() - _serverStartMs) >= _serverTimeoutMs) {
        CONFIG_DEBUG("HTTP server timeout reached, stopping");
        stopConfigServer();
    }
}

static const char CONFIG_PAGE_CSS[] =
    "label[for=yellow],label[for=red]{display:flex;align-items:center;background:#000;padding:0 8px;box-sizing:border-box}"
    ".row.threshold{align-items:stretch}"
    "label[for=yellow]{color:#ff0}"
    "label[for=red]{color:red}"
    ".row.threshold{margin:4px 0}"
    ".row.check{gap:8px}"
    ".row.check label{flex:0 1 auto}"
    ".input-wrapper{position:relative;flex:1 1 auto}"
#if BCP_FEATURE_BRIGHTNESS
    "input[type=range]{flex:1 1 auto}"
#endif
    ".clear-text{display:none}"
    "@media (pointer:coarse){"
        ".input-wrapper input{padding-right:2.5rem}"
        ".clear-text{display:block;position:absolute;right:.7rem;top:50%;transform:translateY(-50%);text-decoration:none;cursor:pointer;color:grey}"
    "}"
    ".version{margin-top:28px;text-align:center;font-size:12px;color:#888}";

static const char CONFIG_PAGE_SCRIPT[] =
    "<script>"
        "function clearInput(el) {"
            "const wrapper=el.closest('.input-wrapper');"
            "const input=wrapper.querySelector('input');"
            "input.value='';"
            "input.focus();"
        "}"
    "</script>";

void CConfig::handleConfigRoot() {
    String status = statusMessage();
    String page = WebUI::buildPageStart("BCP-clock config", CONFIG_PAGE_CSS);
    page.reserve(9000);

    if (status.length() > 0) {
        page += "<div class='status ok'>" + status + "</div>";
    }
    appendConfigFormHtml(page);
    page += CONFIG_PAGE_SCRIPT;
#if BCP_FEATURE_WIFIMGR
    page += WifiMgr.buildStoredWifisHtml();
#endif
    page += "<p class='version'>BCP Clock " BCP_VERSION "</p>";
    page += WebUI::buildPageEnd();

    configServer->send(200, "text/html", page);
}

// Status bar text after a redirect from a form action, empty if none
String CConfig::statusMessage() {
#if BCP_FEATURE_WIFIMGR
    if (configServer->hasArg("wifiAllDeleted")) return "All WiFi networks deleted";
    if (configServer->hasArg("wifiDeleted")) return "WiFi network deleted";
#endif
    if (configServer->hasArg("saved")) return "Configuration saved";
    return "";
}

void CConfig::appendConfigFormHtml(String& html) {
    html += "<form method='POST' action='/config'>";
#if BCP_MODE_BCP
    html += "<div class='row'>"
                    "<label for='event'>Event:</label>"
                    "<div class='input-wrapper'>"
                        "<input type='text' id='event' name='event' placeholder='Enter event ID or event URL' value='" + WebUI::escapeHtml(event_) + "'>"
                        "<a class='clear-text' onclick='clearInput(this);return false;'>⨂</a>"
                    "</div>"
                "</div>";
#endif
    html += WebUI::buildSectionHeader("Colour thresholds");
    html += "<div class='row threshold'>"
                "<label for='yellow'>Yellow:</label>"
                "<input type='text' id='yellow' name='yellow' value='" + secondsToTime(yellow_) + "'>"
            "</div>"
            "<div class='row threshold'>"
                "<label for='red'>Red:</label>"
                "<input type='text' id='red' name='red' value='" + secondsToTime(red_) + "'>"
            "</div>"
            "<div class='row check'>"
                "<input type='checkbox' id='beep' name='beep' value='1'" + (beep_ ? " checked" : "") + ">"
                "<label for='beep'>Acoustic signals</label>"
            "</div>";
#if BCP_MODE_OFFLINE
    html += WebUI::buildSectionHeader("Offline mode");
    html += "<div class='row'>"
                "<label for='offline'>Round:</label>"
                "<input type='text' id='offline' name='offline' value='" + secondsToTime(offline_) + "'>"
            "</div>";
#endif
#if BCP_MODE_CHESS
    html += WebUI::buildSectionHeader("Chess clock");
    html += "<div class='row'>"
                "<label for='chess'>Round:</label>"
                "<input type='text' id='chess' name='chess' value='" + secondsToTime(chess_) + "'>"
            "</div>";
#endif
#if BCP_FEATURE_BRIGHTNESS
    String bright(brightness_);
    html += WebUI::buildSectionHeader("Display");
    html += "<div class='row'>"
                "<label for='brightness'>Brightness:</label>"
                "<input type='range' id='brightness' name='brightness' min='10' max='100' step='10' value='" + bright +
                    "' oninput='bv.value=value+\" %\"'>"
                "<output id='bv'>" + bright + " %</output>"
            "</div>";
#endif
#if BCP_FEATURE_WIFIMGR
    html += WebUI::buildSectionHeader("Network");
    html += "<div class='row'>"
                "<label for='hostname'>Hostname:</label>"
                "<input type='text' id='hostname' name='hostname' maxlength='32' value='" + WebUI::escapeHtml(hostname_) + "'>"
            "</div>";
#endif
    html += "<div class='actions'><button class='btn btnMain' type='submit'>Save</button></div>"
            "</form>";
}

void CConfig::handleConfigPost() {
#if BCP_MODE_BCP
    if (configServer->hasArg("event")) {
        event_ = configServer->arg("event");
    }
#endif
    if (configServer->hasArg("yellow")) {
        yellow_ = timeToSeconds(configServer->arg("yellow"));
        // an unchecked checkbox is not sent, the yellow field tells the form was posted
        beep_ = configServer->hasArg("beep");
    }
    if (configServer->hasArg("red")) {
        red_ = timeToSeconds(configServer->arg("red"));
    }
#if BCP_MODE_OFFLINE
    if (configServer->hasArg("offline")) {
        long offline = timeToSeconds(configServer->arg("offline"));
        if (offline > 0) offline_ = offline;  // keep the previous length on an invalid value
    }
#endif
#if BCP_MODE_CHESS
    if (configServer->hasArg("chess")) {
        long chess = timeToSeconds(configServer->arg("chess"));
        if (chess > 1) chess_ = chess;  // keep the previous length on an invalid value
    }
#endif
#if BCP_FEATURE_BRIGHTNESS
    if (configServer->hasArg("brightness")) {
        brightness_ = normalizeBrightness(configServer->arg("brightness").toInt());
    }
#endif
    if (configServer->hasArg("hostname")) {
        String hostname = sanitizeHostname(configServer->arg("hostname"));
        if (hostname != hostname_) {
            hostname_ = hostname;
#if BCP_FEATURE_MDNS
            CONFIG_DEBUG("Hostname changed, restarting mDNS.");
            MDNS.end();
            startMdns();
#endif
        }
    }
    
    savePreferences();
    
    configServer->sendHeader("Location", "/?saved=1");
    configServer->send(303);

    configUpdated = true;
}

#if BCP_FEATURE_WIFIMGR
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
#endif

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