// Configuration management for BCP-clock
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include "config.h"
#include <Preferences.h>
#include <nvs_flash.h>
#include <wifimgr.h>

#define CONFIG_DEBUG(msg) if (debugOut) { debugOut->print("[Config] "); debugOut->println(msg); }

static const String DEF_EVENT = "";
static const long DEF_YELLOW = 600; // seconds
static const long DEF_RED    = 0;   // seconds
static const String DEF_HOSTNAME = "BCP-clock";
static constexpr const char* CONFIG_NS = "config";

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
        event_ = prefs.getString("event", event_);
        // Prefer integer keys; if missing or type-mismatch, try string fallback
        long y = prefs.getLong("yellow", yellow_);
        long r = prefs.getLong("red", red_);
        String yStr = prefs.getString("yellow", "");
        String rStr = prefs.getString("red", "");
        if (yStr.length() > 0) {
            // parse m[:ss] as minutes/seconds (same as config webpage form)
            y = timeToSeconds(yStr);
        }
        if (rStr.length() > 0) {
            r = timeToSeconds(rStr);
        }
        yellow_   = y;
        red_      = r;
        hostname_ = prefs.getString("hostname", hostname_);
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
    configServer->on("/wifi/delete", HTTP_GET, [this]() { handleWifiDelete(); });
    configServer->on("/wifi/deleteAll", HTTP_GET, [this]() { handleWifiDeleteAll(); });

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
            _serverTimedOut = false; // do not block restarts
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
    
    String page = "<!DOCTYPE html><html><head>"
                  "<title>BCP-clock config</title>"
                  "<meta charset='utf-8'>"
                  "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                  "<style>" + getCss() + "</style>"
                  "</head><body>"
                  "<h1>BCP-clock config</h1>"

                  // status of saving the config
                  "<div class='status'>" + status + "</div>"

                  // the main form of config page
                  "<form method='POST' action='/config'>"
                  "<div id='event' class='row'>"
                  "<label for='event'>Event:</label>"
                  "<input type='text' name='event' placeholder='Enter event ID or event URL' value='" + event_ + "' />"
                  "</div>"
                  "<label class='section'>Colour thresholds</label>"
                  "<div id='yellow' class='row'>"
                  "<label for='yellow'>Yellow:</label>"
                  "<input type='text' name='yellow' value='" + secondsToTime(yellow_) + "' />"
                  "</div>"
                  "<div id='red' class='row'>"
                  "<label for='red'>Red:</label>"
                  "<input type='text' name='red' value='" + secondsToTime(red_) + "' />"
                  "</div>"
                  "<label class='section'>Network</label>"
                  "<div id='hostname' class='row'>"
                  "<label for='hostname'>Hostname:</label>"
                  "<input type='text' name='hostname' value='" + hostname_ + "' />"
                  "</div>"
                  "<input type='submit' />"
                  "</form>";

    // WiFi networks section
    page += "<div id='wifi-networks'>"
            "<label class='section'>Stored WiFi networks</label>";
    std::vector<String> storedNetworks;
    if (WifiMgr.listStoredNetworks(storedNetworks) && !storedNetworks.empty()) {
        page += "<div class='wifi-list'>";
        for (auto& ssid : storedNetworks) {
            String escapedSsid = ssid;
            escapedSsid.replace("'", "\\'");
            page += "<div class='wifi-row'>"
                    "<span class='wifi-ssid'>" + ssid + "</span>"
                    "<a class='wifi-del' href='/wifi/delete?ssid=" + urlEncode(ssid) + "' "
                    "onclick='return confirm(\"Delete WiFi network " + escapedSsid + "?\");'>👉🗑️</a>"
                    "</div>";
        }
        page += "</div>";
        page += "<div class='wifi-actions'>"
                "<a class='wifi-btn-del-all' href='/wifi/deleteAll' "
                "onclick='return confirm(\"Delete ALL stored WiFi networks?\");'>"
                "Delete all stored networks</a>"
                "</div>";
    } else {
        page += "<p>No stored WiFi networks.</p>";
    }

    page += "</div></body></html>";
    
    configServer->send(200, "text/html", page);
}

inline String CConfig::getCss() const {
    return ".status,h1{color:#fff;text-align:center}.status,.wifi-actions,h1,label.section{text-align:center}body{font-family:Arial,Helvetica,sans-serif;margin:0 auto;padding:1em;max-width:51em}h1{background-color:#601860;padding:.4em .3em;margin:.5em 0 .3em;font-size:2.2em;letter-spacing:.5px}.status{background-color:green;display:block;margin:auto auto 1em;width:50%;padding:.5em}div:empty{display:none}form{padding:.5em 0 1.2em}.row{display:flex;align-items:center;margin:22px 0 12px;flex-wrap:nowrap}label{min-width:6.5em;font-size:1.3em}input[type=text]{width:100%;font-size:18px;padding:10px 12px;border:1px solid #888;border-radius:4px}label.section{display:block;margin:1em 0 .5em;border-top:1px solid #000;width:100%;padding:.7em 0 .3em;font-size:x-large;font-weight:400}label[for=red],label[for=yellow]{background:#000;padding:.5em 0 .4em .4em;min-width:6.2em}input[type=button],input[type=submit]{display:block;font-size:x-large;margin:2em auto;padding:14px 24px;cursor:pointer;min-width:60%;max-width:600px;font-weight:700;border:1px solid #777;border-radius:6px}.row#yellow{margin-bottom:2px}.row#red{margin-top:0}label[for=yellow]{color:#ff0}label[for=red]{color:red}.wifi-list{margin:.5em 1em}.wifi-row{display:flex;justify-content:space-between;align-items:center;padding:.5em;border:1px solid #ccc;border-radius:4px;margin-bottom:.5em}.wifi-ssid{overflow:hidden;text-overflow:ellipsis}.wifi-del{text-decoration:none;color:red;font-weight:700;padding:.2em .5em}.wifi-del:hover{background-color:#fee;border-radius:3px}.wifi-actions{margin:1em}.wifi-btn-del-all{display:inline-block;padding:.5em 1em;background-color:#eaeaea;color:#111;text-decoration:none;border:1px solid #777;border-radius:4px;cursor:pointer}.wifi-btn-del-all:hover{filter:brightness(.95)}@media (max-width:700px){body{max-width:22em}h1{font-size:1.8em;padding:.5em 0;margin-bottom:.5em}form{padding:0;font-size:1.5em}.row#event{flex-wrap:wrap;margin-top:.5em}.row label{min-width:0;max-width:5.5em;padding-top:.4em;padding-bottom:.4em}.row#red label,.row#yellow label{max-width:5.2em}label{width:100%;font-size:.9em}label.section{margin:.5em 0 0;padding-bottom:0}input[type=text]{width:94%;border-color:#000}input[type=button],input[type=submit]{font-size:1em;width:100%}#wifi-networks label.section{padding:1em 0}.wifi-list{margin:.5em 0}.wifi-row{font-size:1.2em}.wifi-btn-del-all{font-size:1.2em;padding:.5em 1em;width:90%}}";
}

void CConfig::handleConfigPost() {
    if (configServer->hasArg("event")) {
        event_ = urlDecode(configServer->arg("event"));
    }
    if (configServer->hasArg("yellow")) {
        yellow_ = timeToSeconds(urlDecode(configServer->arg("yellow")));
    }
    if (configServer->hasArg("red")) {
        red_ = timeToSeconds(urlDecode(configServer->arg("red")));
    }
    if (configServer->hasArg("hostname")) {
        hostname_ = urlDecode(configServer->arg("hostname"));
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
    String ssid = urlDecode(configServer->arg("ssid"));
    CONFIG_DEBUG("Request to delete WiFi network '" + ssid + "'");

    if (WifiMgr.removeStoredNetwork(ssid.c_str())) {
        configServer->sendHeader("Location", "/?wifiDeleted=1");
        configServer->send(302);
    } else {
        configServer->send(200, "text/plain", "Delete failed or network not found");
    }
}

void CConfig::handleWifiDeleteAll() {
    CONFIG_DEBUG("Request to delete ALL stored WiFi networks");
    if (WifiMgr.eraseStoredNetworks()) {
        configServer->sendHeader("Location", "/?wifiAllDeleted=1");
        configServer->send(302);
    } else {
        configServer->send(200, "text/plain", "Delete all failed");
    }
}

String CConfig::urlDecode(const String& str) const {
    String decoded = "";
    char c;
    char code0;
    char code1;
    for (unsigned int i = 0; i < str.length(); i++) {
        c = str.charAt(i);
        if (c == '+') {
            decoded += ' ';
        } else if (c == '%') {
            if (i + 2 < str.length()) {
                code0 = str.charAt(++i);
                code1 = str.charAt(++i);
                c = (h2int(code0) << 4) | h2int(code1);
                decoded += c;
            }
        } else {
            decoded += c;
        }
    }
    return decoded;
}

String CConfig::urlEncode(const String& in) {
    String out;
    out.reserve(in.length() * 3);
    const char* hex = "0123456789ABCDEF";
    for (size_t i = 0; i < in.length(); ++i) {
        uint8_t c = (uint8_t)in[i];
        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            out += (char)c;
        } else {
            out += '%';
            out += hex[(c >> 4) & 0xF];
            out += hex[c & 0xF];
        }
    }
    return out;
}

// ---- Helpers ----

String CConfig::secondsToTime(long seconds) const {
    long m = seconds / 60;
    long s = seconds % 60;
    char buf[16];
    snprintf(buf, sizeof(buf), "%ld:%02ld", m, s);
    return String(buf);
}

long CConfig::timeToSeconds(const String& time) const {
    String t = time;
    t.replace(".", ":");  // if time entered as 05.00, replace dots by colons
    t.trim();

    int colonPos = t.indexOf(':');
    if (colonPos == -1) {
        // Interpret single number as minutes
        long m = t.toInt();
        return m * 60;
    }

    long m = t.substring(0, colonPos).toInt();
    long s = t.substring(colonPos + 1).toInt();
    return m * 60 + s;
}

// Add function used by urlDecode()
inline uint8_t CConfig::h2int(char c) {
    if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
    if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(10 + c - 'a');
    if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(10 + c - 'A');
    return 0; // invalid hex char
}

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