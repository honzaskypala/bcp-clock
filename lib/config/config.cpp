// Configuration management for BCP-clock
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include "config.h"
#include <Preferences.h>
#include <nvs_flash.h>

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
        if (Serial) Serial.println("Config: preferences loaded");
    } else {
        if (Serial) Serial.println("Config: failed to open preferences for reading");
    }
}

// ---- Save configuration to persistent storage ----

void CConfig::savePreferences() {
    if (!ensureNvsReady()) {
        if (Serial) Serial.println("Config: NVS not ready for writing");
        return;
    }
    Preferences prefs;
    if (prefs.begin(CONFIG_NS, false)) { // write mode
        Serial.println("Config: saving preferences");
        prefs.putString("event", event_);
        prefs.putLong("yellow", yellow_);
        prefs.putLong("red", red_);
        prefs.putString("hostname", hostname_);
        prefs.end();
        Serial.println("Config: preferences saved");
    } else {
        Serial.println("Config: failed to open preferences for writing");
    }
}

// ---- Erase configuration from persistent storage ----

void CConfig::erase() {
    if (!ensureNvsReady()) {
        if (Serial) Serial.println("Config: NVS not ready for clearing");
        return;
    }
    Preferences prefs;
    if (prefs.begin(CONFIG_NS, false)) { // write mode
        Serial.println("Config: clearing preferences");
        prefs.clear();
        prefs.end();
        Serial.println("Config: preferences cleared");
        // Reset to defaults
        event_ = DEF_EVENT;
        yellow_ = DEF_YELLOW;
        red_ = DEF_RED;
        hostname_ = DEF_HOSTNAME;
    } else {
        if (Serial) Serial.println("Config: failed to open preferences for clearing");
    }
}

// ---- Configuration HTTP server ----

void CConfig::startConfigServer(bool atStartup, unsigned long timeoutMs) {
    // If already running, just update timeout and reset the start time
    if (configServer != nullptr) {
        _serverTimeoutMs = timeoutMs;      // 0 => no timeout
        _serverStartMs = millis();         // reset timer window
        return;
    }
    atStartup_ = atStartup;
    
    configServer = new WebServer(80);
    
    configServer->on("/", HTTP_GET, [this]() { handleConfigRoot(); });
    configServer->on("/config", HTTP_POST, [this]() { handleConfigPost(); });

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
    Serial.print("Config: HTTP server started on http://");
    Serial.print(WiFi.localIP());
    if (_serverTimeoutMs == 0) {
        Serial.println("/ (no timeout)");
    } else {
        Serial.println("/ (timeout active)");
    }
}

void CConfig::stopConfigServer() {
    if (configServer != nullptr) {
        configServer->stop();
        delete configServer;
        configServer = nullptr;
        Serial.println("Config: HTTP server stopped");
    }
}

void CConfig::handleClient() {
    if (configServer != nullptr) {
        configServer->handleClient();
        // Stop server after timeout, if configured
        if (_serverTimeoutMs > 0 && (millis() - _serverStartMs) >= _serverTimeoutMs) {
            Serial.println("Config: HTTP server timeout reached, stopping");
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
    
    String page = "<!DOCTYPE html><html><head>"
                  "<title>BCP-clock config</title>"
                  "<meta charset='utf-8'>"
                  "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                  "<style>" + getCss() + "</style>"
                  "</head><body>"
                  "<h1>BCP-clock config</h1>";
    
    if (status.length() > 0) {
        page += "<div class='status'>" + status + "</div>";
    }
    
    if (event_ == "" || !configServer->hasArg("saved") || !atStartup_) {
        page += "<form method='POST' action='/config'>"
                "<div id='event'>"
                "<label for='event'>Event:</label>"
                "<input type='text' name='event' value='" + event_ + "' />"
                "</div>"
                "<label class='section'>Colour thresholds</label>"
                "<div id='yellow'>"
                "<label for='yellow'>Yellow:</label>"
                "<input type='text' name='yellow' value='" + secondsToTime(yellow_) + "' />"
                "</div>"
                "<div id='red'>"
                "<label for='red'>Red:</label>"
                "<input type='text' name='red' value='" + secondsToTime(red_) + "' />"
                "</div>"
                "<label class='section'>Network</label>"
                "<div id='hostname'>"
                "<label for='hostname'>Hostname:</label>"
                "<input type='text' name='hostname' value='" + hostname_ + "' />"
                "</div>"
                "<input type='submit' />"
                "</form>";
    }
    page += "</body></html>";
    
    configServer->send(200, "text/html", page);
}

inline String CConfig::getCss() const {
    return "body {font-family: Arial, Helvetica, sans-serif; display: block; margin: 0.5em auto; max-width: 20em}"
           "h1 {background-color: rgb(96, 24, 96); color: white; text-align: center; padding: 0 20px; margin-top: 0}"
           ".status {background-color: green; color: white; display: block; margin: auto auto 1em; width: 50%; padding: 0.5em; text-align: center}"
           "div:empty {display: none}"
           "form {padding: 0 1em; font-size: medium}"
           "label {display: inline-block; min-width: 5.5em; font-weight: bold}"
           "input[type=text] {display: inline-block; min-width: 14em; margin-bottom: 0.2em}"
           "label.section {display: block; text-align: center; margin: 1em 0 0.5em; border-top: 1px solid black; width: 100%; padding: 0.7em 0 0.3em; font-size: large}"
           "input[type=submit], input[type=button] {display: block; width: 100%; font-size: large; margin-top: 1em}"
           "label[for=yellow] {background: black; color: yellow; padding-left: 0.2em; min-width: 5.3em}"
           "label[for=red] {background: black; color: red; padding-left: 0.2em; min-width: 5.3em}"
           "@media (max-width: 700px) {"
                "body {max-width: 22em}"
                "h1 {font-size: 1.8em; padding: 0.5em 0em; margin-bottom: 0.5em}"
                "form {padding: 0; font-size: 1.5em}"
                "label {width: 97%}"
                "label.section {font-size: 1.2em; margin: 0.5em 0 0.2em}"
                "input[type=text] {display: inline-block; width: 94%; margin-bottom: 0.2em; font-size: 1em; border-color: black}"
                "input[type=submit], input[type=button] {font-size: 1em; padding: 0.3em}"
           "}";
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

    if (!atStartup_) {
        configUpdated = true;
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
        Serial.println("Config: NVS init failed");
    }
    return ready;
}

// ---- Singleton related ----

CConfig& getConfigInstance() {
    static CConfig instance;
    return instance;
}

CConfig& Config = getConfigInstance();