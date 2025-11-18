// ESP32 WiFi Manager with captive portal for BCP clock
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include "wifimgr.h"

#include <WiFi.h>
#include <time.h>
#include <esp_wifi.h>
#include <framebuffer.h>
#include <vector>

// ---------- Config ----------
static const uint32_t CONNECT_TIMEOUT_MS = 15000;
static const uint32_t PORTAL_CONNECT_TIMEOUT_MS = 20000;
static constexpr const char* AP_SSID = "BCP-clock";
static const char* DEFAULT_AP_PASSWORD = "";
static const byte DNS_PORT = 53;
static const char* NTP_SERVER = "pool.ntp.org";
static const long GMT_OFFSET_SEC = 0;
static const int DAYLIGHT_OFFSET_SEC = 3600;
static constexpr const char* PREFS_NS       = "wificreds";
static constexpr const char* PREFS_LIST_KEY = "__ssids";

// -----------Singleton----------
CWifiMgr WifiMgr;

// ---------- Public API ----------
bool CWifiMgr::connect(uint32_t portalTimeoutMs) {
    Serial.begin(9600);
    while (!Serial) {}

    Serial.println("WifiMgr: Attempting automatic WiFi connection.");
    connected_ = false;

    if (!scanAndSort()) {
        Serial.println("WifiMgr: Scan failed or found no networks.");
    } else {
        // Pass 1: known networks (DO NOT save credential here)
        std::vector<String> storedSsids;
        bool haveCreds = listStoredSsids(storedSsids);

        if (haveCreds) {
            Serial.println("WifiMgr: Pass 1 - Trying known networks...");
            for (int order = 0; order < scanCount_; ++order) {
                int i = scanIdx_[order];
                const String& ssid = scanSsid_[i];
                Serial.println("WifiMgr: Considering SSID: " + ssid);
                if (ssid.length() == 0) continue;

                String pass;
                if (getCredential(ssid.c_str(), pass)) {
                    Serial.printf("WifiMgr: Known SSID candidate: %s (RSSI %d dBm)\n",
                                  ssid.c_str(), scanRssi_[order]);

                    if (attemptConnect(ssid.c_str(), pass.c_str(), CONNECT_TIMEOUT_MS)) {
                        connected_ = true;
                        freeScanData();
                        Serial.println("WifiMgr: Connected via known network (no credential save).");
                        return true;
                    }
                }
                delay(0);
            }
            Serial.println("WifiMgr: Pass 1 finished: No known networks connected.");
        } else {
            Serial.println("WifiMgr: No known networks (missing or invalid /wifi.json).");
        }

        // Pass 2: open networks (DO NOT save credential here)
        Serial.println("WifiMgr: Pass 2 - Trying open networks...");
        for (int order = 0; order < scanCount_; ++order) {
            int i = scanIdx_[order];
            uint8_t enc = scanEnc_[i];
            if (!isOpenEncryption(enc)) continue;
            const String& ssid = scanSsid_[i];
            if (ssid.length() == 0) continue;

            Serial.printf("WifiMgr: Open SSID candidate: %s (RSSI %d dBm)\n",
                          ssid.c_str(), scanRssi_[order]);

            if (attemptConnect(ssid.c_str(), nullptr, CONNECT_TIMEOUT_MS)) {
                connected_ = true;
                freeScanData();
                Serial.println("WifiMgr: Connected via open network (no credential save).");
                return true;
            }
            delay(0);
        }
        Serial.println("WifiMgr: Pass 2 finished: No open networks connected.");
    }

    Serial.println("WifiMgr: Launching captive configuration portal (blocking until connected or timeout).");
    startConfigPortal();
    uint32_t portalStart = millis();

    while (!connected_) {
        dnsServer_.processNextRequest();
        server_.handleClient();
        delay(10);
        if (portalTimeoutMs > 0 && (millis() - portalStart) > portalTimeoutMs) {
            Serial.println("WifiMgr: Portal timeout reached, exiting without WiFi.");
            break;
        }
    }

    freeScanData();
    return connected_;
}

// ---------- Core steps ----------
bool CWifiMgr::scanAndSort() {
    ensureStaModeForScan();

    Serial.println();
    Serial.println("WifiMgr: Preparing for WiFi scan...");
    if (!portalActive_) {
        Serial.println("WifiMgr: Waiting 3 seconds before scan to avoid early boot scan issues...");
        delay(3000);
    } else {
        Serial.println("WifiMgr: Portal active - skipping initial scan delay to keep AP responsive.");
        delay(50);
    }

    Serial.println("WifiMgr: Starting WiFi scan...");
    int n = WiFi.scanNetworks();
    Serial.println("WifiMgr: Scan done");

    if (n <= 0) {
        Serial.println("WifiMgr: No networks found");
        return false;
    }

    uint16_t count = static_cast<uint16_t>(n);
    wifi_ap_record_t* recs = static_cast<wifi_ap_record_t*>(malloc(count * sizeof(wifi_ap_record_t)));
    bool usedDriverRecords = false;

    if (recs) {
        if (esp_wifi_scan_get_ap_records(&count, recs) == ESP_OK && count > 0) {
            scanCount_ = static_cast<int>(count);
            scanIdx_   = new int[scanCount_];
            scanRssi_  = new int[scanCount_];
            scanSsid_  = new String[scanCount_];
            scanEnc_   = new uint8_t[scanCount_];
            scanChan_  = new int[scanCount_];

            Serial.println("WifiMgr: Scan results (driver):");
            for (int i = 0; i < scanCount_; ++i) {
                scanIdx_[i]  = i;
                scanSsid_[i] = String(reinterpret_cast<const char*>(recs[i].ssid));
                scanRssi_[i] = static_cast<int>(recs[i].rssi);
                scanEnc_[i]  = static_cast<uint8_t>(recs[i].authmode);
                scanChan_[i] = static_cast<int>(recs[i].primary);

                const uint8_t* b = recs[i].bssid;
                Serial.printf("  [%d] SSID='%s' RSSI=%d dBm ENC=%u CH=%d BSSID=%02X:%02X:%02X:%02X:%02X:%02X\n",
                              i,
                              scanSsid_[i].c_str(),
                              scanRssi_[i],
                              (unsigned)scanEnc_[i],
                              scanChan_[i],
                              b[0], b[1], b[2], b[3], b[4], b[5]);
            }
            usedDriverRecords = true;
        }
        free(recs);
    }

    if (!usedDriverRecords) {
        scanCount_ = n;
        scanIdx_   = new int[scanCount_];
        scanRssi_  = new int[scanCount_];
        scanSsid_  = new String[scanCount_];
        scanEnc_   = new uint8_t[scanCount_];
        scanChan_  = new int[scanCount_];

        Serial.println("WifiMgr: Scan results (fallback API):");
        for (int i = 0; i < scanCount_; ++i) {
            scanIdx_[i]  = i;
            scanSsid_[i] = WiFi.SSID(i);
            scanRssi_[i] = WiFi.RSSI(i);
            scanEnc_[i]  = (uint8_t)WiFi.encryptionType(i);
            scanChan_[i] = WiFi.channel(i);
            Serial.printf("  [%d] SSID='%s' RSSI=%d dBm ENC=%u CH=%d\n",
                          i,
                          scanSsid_[i].c_str(),
                          scanRssi_[i],
                          (unsigned)scanEnc_[i],
                          scanChan_[i]);
        }
    }

    WiFi.scanDelete();

    // Sort by RSSI descending (keep idx indirection)
    for (int i = 1; i < scanCount_; ++i) {
        int keyIdx  = scanIdx_[i];
        int keyRssi = scanRssi_[i];
        int j = i - 1;
        while (j >= 0 && scanRssi_[j] < keyRssi) {
            scanIdx_[j + 1]  = scanIdx_[j];
            scanRssi_[j + 1] = scanRssi_[j];
            --j;
        }
        scanIdx_[j + 1]  = keyIdx;
        scanRssi_[j + 1] = keyRssi;
    }

    Serial.printf("WifiMgr: %d networks found (sorted strongest to weakest)\n", scanCount_);
    return true;
}

void CWifiMgr::freeScanData() {
    if (scanIdx_) { delete[] scanIdx_; scanIdx_ = nullptr; }
    if (scanRssi_) { delete[] scanRssi_; scanRssi_ = nullptr; }
    if (scanSsid_) { delete[] scanSsid_; scanSsid_ = nullptr; }
    if (scanEnc_) { delete[] scanEnc_; scanEnc_ = nullptr; }
    if (scanChan_) { delete[] scanChan_; scanChan_ = nullptr; }
    scanCount_ = 0;
    WiFi.scanDelete();
}

// ---------- Connection attempt ----------
bool CWifiMgr::attemptConnect(const char* ssid, const char* pass, uint32_t timeoutMs) {
    if (!ssid || ssid[0] == '\0') return false;

    Serial.print("WifiMgr: Trying to connect to SSID: ");
    Serial.println(ssid);

    WiFi.disconnect(true, true);
    delay(50);

    if (pass && pass[0] != '\0') {
        WiFi.begin(ssid, pass);
    } else {
        WiFi.begin(ssid);
    }

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
        delay(150);
        delay(0);
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("WifiMgr: Connected. IP address: ");
        Serial.println(WiFi.localIP());

        if (!syncTime(10000)) {
            Serial.println("WifiMgr: NTP sync failed. Disconnecting and failing connect attempt.");
            WiFi.disconnect(true, true);
            FrameBuffer.progressStop();
            FrameBuffer.textScrollStop();
            FrameBuffer.textScroll("Error: NTP time sync failed", 2, "f3x5", CRGB::Red, CRGB::Black, 24);
            timeSyncFailed_ = true;
            delay(100);
            return false;
        }

        return true;
    }
    Serial.println("WifiMgr: Connection attempt failed.");
    return false;
}

// ---------- Credentials load/save/remove ----------
bool CWifiMgr::beginPrefs(bool readOnly) {
    // Return true on success, false on failure
    return prefs_.begin(PREFS_NS, readOnly);
}

// FNV-1a 32-bit hash for key shortening (SSID can be arbitrary)
static uint32_t fnv1a32(const String& s) {
    uint32_t h = 2166136261UL;
    for (size_t i = 0; i < s.length(); ++i) {
        h ^= (uint8_t)s[i];
        h *= 16777619UL;
    }
    return h;
}

String CWifiMgr::makePassKey(const String& ssid) {
    uint32_t h = fnv1a32(ssid);
    char buf[13]; // "pwd_" + 8 hex + null
    snprintf(buf, sizeof(buf), "pwd_%08x", (unsigned)h);
    return String(buf);
}

void CWifiMgr::splitList(const String& src, std::vector<String>& out) {
    out.clear();
    int start = 0;
    while (start <= (int)src.length()) {
        int nl = src.indexOf('\n', start);
        if (nl < 0) nl = src.length();
        String item = src.substring(start, nl);
        if (item.length() > 0) out.push_back(item);
        start = nl + 1;
    }
}

String CWifiMgr::joinList(const std::vector<String>& list) {
    String s;
    for (size_t i = 0; i < list.size(); ++i) {
        s += list[i];
        if (i + 1 < list.size()) s += '\n';
    }
    return s;
}

bool CWifiMgr::listStoredSsids(std::vector<String>& out) {
    out.clear();
    if (!beginPrefs(true)) {
        Serial.println("WifiMgr: Preferences begin (RO) failed.");
        return false;
    }

    String list;
    if (prefs_.isKey(PREFS_LIST_KEY)) {
        list = prefs_.getString(PREFS_LIST_KEY, "");
    } else {
        list = "";
    }
    prefs_.end();

    splitList(list, out);
    if (!out.empty()) {
        Serial.println("WifiMgr: Loaded stored SSIDs from Preferences:");
        for (auto& s : out) Serial.printf("  SSID: %s\n", s.c_str());
        return true;
    }
    Serial.println("WifiMgr: No stored SSIDs in Preferences.");
    return false;
}

bool CWifiMgr::getCredential(const char* ssid, String& outPass) {
    if (!ssid || !beginPrefs(true)) return false;

    String key = makePassKey(String(ssid));
    bool ok = false;
    if (prefs_.isKey(key.c_str())) {
        outPass = prefs_.getString(key.c_str(), "");
        ok = true; // can be empty for OPEN
    }
    prefs_.end();
    return ok;
}

bool CWifiMgr::saveCredential(const char* ssid, const char* pass) {
    if (!ssid || ssid[0] == '\0') {
        Serial.println("WifiMgr: saveCredential: Empty SSID, skipping.");
        return false;
    }
    if (!beginPrefs(false)) {
        Serial.println("WifiMgr: saveCredential: Preferences begin (RW) failed.");
        return false;
    }

    // Read current list only if it exists to avoid NOT_FOUND log
    String listStr;
    if (prefs_.isKey(PREFS_LIST_KEY)) {
        listStr = prefs_.getString(PREFS_LIST_KEY, "");
    } else {
        listStr = "";
    }

    std::vector<String> list;
    splitList(listStr, list);
    bool found = false;
    for (auto& s : list) if (s == ssid) { found = true; break; }
    if (!found) list.push_back(String(ssid));
    prefs_.putString(PREFS_LIST_KEY, joinList(list));

    String key = makePassKey(String(ssid));
    prefs_.putString(key.c_str(), (pass ? pass : ""));

    prefs_.end();
    Serial.printf("WifiMgr: saveCredential: Stored credential for '%s'.\n", ssid);
    return true;
}

bool CWifiMgr::removeCredential(const char* ssid) {
    if (!ssid || ssid[0] == '\0') {
        Serial.println("WifiMgr: removeCredential: Empty SSID, skipping.");
        return false;
    }
    if (!beginPrefs(false)) {
        Serial.println("WifiMgr: removeCredential: Preferences begin (RW) failed.");
        return false;
    }

    // Read list only if present (avoid NOT_FOUND log)
    String listStr;
    if (prefs_.isKey(PREFS_LIST_KEY)) {
        listStr = prefs_.getString(PREFS_LIST_KEY, "");
    } else {
        listStr = "";
    }

    std::vector<String> list;
    splitList(listStr, list);
    bool removed = false;
    std::vector<String> out;
    out.reserve(list.size());
    for (auto& s : list) {
        if (s == ssid) { removed = true; continue; }
        out.push_back(s);
    }
    if (!removed) {
        prefs_.end();
        Serial.printf("WifiMgr: removeCredential: SSID '%s' not found.\n", ssid);
        return false;
    }
    prefs_.putString(PREFS_LIST_KEY, joinList(out));

    String key = makePassKey(String(ssid));
    if (prefs_.isKey(key.c_str())) {
        prefs_.remove(key.c_str());
    }

    prefs_.end();
    Serial.printf("WifiMgr: removeCredential: Removed '%s'.\n", ssid);
    return true;
}

// ---------- Mode / Time ----------
void CWifiMgr::ensureStaModeForScan() {
    // Keep configuration in RAM only
    WiFi.persistent(false);

    wifi_mode_t cur = WiFi.getMode();

    if (portalActive_) {
        // Portal is active: keep AP up, ensure STA is enabled for scanning.
        // Do NOT call WiFi.disconnect(true, true) here, it would drop the AP and HTTP clients.
        if (cur != WIFI_MODE_APSTA) {
            WiFi.mode(WIFI_MODE_APSTA);
        }
        // If STA is currently associated, you may optionally disconnect only the STA side
        // to speed up scanning, but keep AP alive:
        // esp_wifi_disconnect(); // affects STA only, AP stays up
        delay(50);
    } else {
        // Normal boot-time scan: STA-only and fully reset associations
        WiFi.mode(WIFI_MODE_STA);
        WiFi.disconnect(true, true);
        delay(100);
    }
}

bool CWifiMgr::syncTime(uint32_t timeoutMs) {
    Serial.println("WifiMgr: Syncing time via NTP...");
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);

    struct tm timeinfo;
    if (getLocalTime(&timeinfo, timeoutMs)) {
        Serial.println("WifiMgr: Time synchronized:");
        printLocalTime();
        return true;
    } else {
        Serial.println("WifiMgr: Failed to obtain time.");
        return false;
    }
}

void CWifiMgr::printLocalTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("WifiMgr: Failed to obtain time.");
        return;
    }
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %Z", &timeinfo);
    Serial.printf("WifiMgr: Local time: %s\n", buf);
}

// ---------- HTML helpers ----------
String CWifiMgr::htmlEscape(const String& in) {
    String out;
    out.reserve(in.length() + 10);
    for (size_t i = 0; i < in.length(); ++i) {
        char c = in[i];
        switch (c) {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&#39;";  break;
            default:   out += c;        break;
        }
    }
    return out;
}

String CWifiMgr::urlEncode(const String& in) {
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

const char* CWifiMgr::wifiAuthModeToString(uint8_t m) {
    // Cast to known enum for safety
    wifi_auth_mode_t mode = static_cast<wifi_auth_mode_t>(m);
    switch (mode) {
        case WIFI_AUTH_OPEN:            return "OPEN";
        case WIFI_AUTH_WEP:             return "WEP";
        case WIFI_AUTH_WPA_PSK:         return "WPA";
        case WIFI_AUTH_WPA2_PSK:        return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/WPA2";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-Ent";
#if defined(WIFI_AUTH_WPA3_PSK)
        case WIFI_AUTH_WPA3_PSK:        return "WPA3";
#endif
#if defined(WIFI_AUTH_WPA2_WPA3_PSK)
        case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2/WPA3";
#endif
#if defined(WIFI_AUTH_WAPI_PSK)
        case WIFI_AUTH_WAPI_PSK:        return "WAPI";
#endif
#if defined(WIFI_AUTH_OWE)
        case WIFI_AUTH_OWE:             return "OWE";
#endif
#if defined(WIFI_AUTH_WPA3_ENT_192)
        case WIFI_AUTH_WPA3_ENT_192:    return "WPA3-Ent-192";
#endif
        default:
            return "SECURED";
    }
}

// ---------- HTTP Handlers ----------
void CWifiMgr::handleRoot() {
    bool showFail = server_.hasArg("fail");

    std::vector<String> storedSsids;
    bool haveCreds = listStoredSsids(storedSsids);

    String page;
    page.reserve(17000);
    page += "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Wi-Fi Setup</title>";
    page += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
    page += "<style>"
            "html,body{margin:0;padding:0;background:#fff;color:#111;font-family:Arial,Helvetica,sans-serif;}"
            ".wrap{padding:18px;max-width:820px;margin:0 auto;}"
            ".banner{background:#5b1b66;color:#fff;font-weight:800;text-align:center;"
                "padding:14px 10px;margin:18px 0 10px 0;font-size:36px;line-height:1.15;letter-spacing:0.5px;}"
            ".status{font-size:18px;font-weight:600;text-align:center;padding:10px 12px;border-radius:4px;margin:0 0 18px 0;}"
            ".status-error{background:#c3002f;color:#fff;}"
            "fieldset{border:0;padding:0;margin:0 0 20px 0;}"
            "legend{display:none;}"
            "label.ssid{display:flex;align-items:center;gap:10px;font-size:22px;margin:10px 0;}"
            "input[type=radio]{width:20px;height:20px;}"
            ".rssi{color:#666;font-size:12px;margin-left:8px;}"
            ".row{display:flex;align-items:center;gap:16px;margin:22px 0 12px 0;flex-wrap:nowrap;}"
            ".row label{flex:0 0 120px;font-size:20px;}"
            "input[type=password]{flex:1 1 auto;width:100%;font-size:18px;padding:10px 12px;"
                "border:1px solid #888;border-radius:4px;box-sizing:border-box;}"
            ".actions{margin-top:26px;display:flex;justify-content:center;}"
            ".btnSubmit{display:inline-flex;align-items:center;justify-content:center;min-width:60%;max-width:600px;"
                "font-size:26px;font-weight:700;padding:14px 24px;border-radius:6px;border:1px solid #777;"
                "background:#eaeaea;color:#111;text-align:center;text-decoration:none;cursor:pointer;}"
            ".btnSubmit:disabled{opacity:0.65;cursor:default;}"
            "@keyframes spin{to{transform:rotate(360deg);}}"
            ".btnSubmit .waitIcon{display:none;margin-left:10px;}"
            ".btnSubmit.spinning .waitIcon{display:inline-block;animation:spin .8s linear infinite;}"
            ".btnSubmit.spinning .btnText{opacity:0.8;}"
            ".netHeader{display:flex;justify-content:space-between;align-items:center;margin:6px 0 4px 0;}"
            ".sectionLabel{font-size:18px;font-weight:700;color:#222;}"
            ".rescanBtn{font-size:14px;font-weight:600;padding:6px 14px;border:1px solid #777;"
                "background:#eaeaea;color:#111;border-radius:4px;cursor:pointer;display:flex;align-items:center;gap:6px;}"
            ".rescanBtn:hover{filter:brightness(0.95);}"
            ".rescanBtn .icon{display:inline-block;}"
            ".rescanBtn.spinning .icon{animation:spin .8s linear infinite;}"
            ".storedBlock{margin-top:34px;}"
            ".storedLabel{font-size:18px;font-weight:700;margin:4px 0 10px 0;color:#222;}"
            ".storedRow{display:flex;justify-content:space-between;align-items:center;"
                "border:1px solid #ccc;border-radius:4px;padding:8px 12px;margin-bottom:8px;}"
            ".storedSSID{font-size:16px;font-weight:600;overflow:hidden;text-overflow:ellipsis;}"
            "a.del{display:inline-block;text-decoration:none;font-size:22px;line-height:1;}"
            "a.del:hover{filter:brightness(0.85);}"
            "@media (max-width:580px){"
                ".banner{font-size:30px;padding:12px 8px;margin:12px 0 10px 0;}"
                ".row{flex-wrap:wrap;}"
                ".row label{flex:0 0 auto;margin-bottom:6px;}"
                "input[type=password]{width:100%;}"
                ".btnSubmit{min-width:100%;max-width:none;width:100%;}"
                ".sectionLabel{font-size:16px;}"
                ".status{font-size:16px;}"
            "}"
            "</style>";
    page += "</head><body><div class='wrap'>";
    page += "<div class='banner'>BCP-clock Wi‑Fi Setup</div>";

    if (showFail) {
        page += "<div class='status status-error'>Connection failed</div>";
    }

    page += "<form id='wifiForm' method='POST' action='/connect'"
            " onsubmit=\"var b=this.querySelector('.btnSubmit'); if(b){b.classList.add('spinning'); b.disabled=true;} return true;\">"
            "<fieldset><legend>Networks</legend>";

    page += "<div class='netHeader'>"
            "<span class='sectionLabel'>Found networks:</span>"
            "<button class='rescanBtn' type='button' onclick='onRescanClick(this)'><span class='icon'>🔄</span><span>Rescan</span></button>"
            "</div>";

    if (scanCount_ == 0) {
        page += "<p>No scanned networks available.</p>";
    } else {
        for (int order = 0; order < scanCount_; ++order) {
            int idx = scanIdx_[order];
            const String& ssid = scanSsid_[idx];
            if (ssid.length() == 0) continue;
            int rssi = scanRssi_[order];
            uint8_t enc = scanEnc_[idx];
            const char* sec = wifiAuthModeToString(enc);

            page += "<label class='ssid'>";
            page += "<input type='radio' name='ssid' value='" + htmlEscape(ssid) + "'" + (order == 0 ? " checked" : "") + ">";
            page += "<span>" + htmlEscape(ssid) + "</span>";
            page += "<span class='rssi'>(" + String(sec) + ", " + String(rssi) + " dBm)</span>";
            page += "</label>";
        }
        page += "<div class='row'><label for='pwd'>Password:</label>"
                "<input id='pwd' type='password' name='pwd' placeholder='Leave empty for OPEN network'></div>";
        page += "<div class='actions'>"
                "<button class='btnSubmit' type='submit'><span class='btnText'>Submit</span><span class='waitIcon' aria-hidden='true'>⏳</span></button>"
                "</div>";
    }
    page += "</fieldset></form>";

    // Stored networks
    page += "<div class='storedBlock'>";
    page += "<div class='storedLabel'>Stored networks:</div>";
    if (haveCreds && !storedSsids.empty()) {
        for (auto& s : storedSsids) {
            page += "<div class='storedRow'>"
                    "<div class='storedSSID'>" + htmlEscape(s) + "</div>"
                    "<a class='del' href='/delete?ssid=" + urlEncode(s) +
                    "' title='Delete' onclick='return confirm(\"Delete SSID " + htmlEscape(s) + "?\");'>👉🗑️</a>"
                    "</div>";
        }
    } else {
        page += "<p>No networks stored.</p>";
    }
    page += "</div>";

    page += "<script>"
            "function onRescanClick(btn){"
                "if(btn.classList.contains('spinning')) return;"
                "btn.classList.add('spinning');"
                "btn.disabled=true;"
                "void btn.offsetWidth;"
                "requestAnimationFrame(function(){setTimeout(function(){window.location.href='/rescan';},100);});"
            "}"
            "</script>";

    page += "</div></body></html>";
    server_.send(200, "text/html", page);
}

void CWifiMgr::handleRescan() {
    Serial.println("WifiMgr: Portal - Rescanning networks...");
    if (scanAndSort()) {
        server_.sendHeader("Location", "/", true);
        server_.send(302, "text/plain", "Rescanned. Redirecting...");
    } else {
        server_.send(200, "text/plain", "Rescan failed. No networks found.");
    }
}

void CWifiMgr::handleConnect() {
    if (!server_.hasArg("ssid")) {
        server_.sendHeader("Location", "/?fail=1", true);
        server_.send(302, "text/plain", "Missing SSID");
        return;
    }
    String ssid = server_.arg("ssid");
    String pwd  = server_.hasArg("pwd") ? server_.arg("pwd") : "";

    Serial.print("WifiMgr: Portal - User selected SSID: ");
    Serial.println(ssid);
    Serial.println("WifiMgr: Portal - Attempting connection...");

    if (attemptConnect(ssid.c_str(), pwd.c_str(), PORTAL_CONNECT_TIMEOUT_MS)) {
        connected_ = true;
        saveCredential(ssid.c_str(), pwd.c_str());

        String resp;
        resp += "<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>Connected</title></head><body>";
        resp += "<h1>Connected successfully!</h1>";
        resp += "<p>IP: " + WiFi.localIP().toString() + "</p>";
        resp += "<p>Credential saved. Configuration portal will shut down.</p>";
        resp += "</body></html>";
        server_.send(200, "text/html", resp);
        delay(500);
        stopPortal();
    } else {
        // Redirect back to root with fail flag so root shows status bar
        server_.sendHeader("Location", "/?fail=1", true);
        server_.send(302, "text/plain", "Connection failed");
    }
}

void CWifiMgr::handleDelete() {
    if (!server_.hasArg("ssid")) {
        server_.send(400, "text/plain", "Missing ssid parameter");
        return;
    }
    String ssid = server_.arg("ssid");
    Serial.printf("WifiMgr: Portal - Request to delete SSID '%s'.\n", ssid.c_str());

    bool ok = removeCredential(ssid.c_str());
    if (ok) {
        server_.sendHeader("Location", "/", true);
        server_.send(302, "text/plain", "Deleted. Redirecting...");
    } else {
        server_.send(200, "text/plain", "Delete failed or SSID not found. Return to /.");
    }
}

void CWifiMgr::handleCaptive() {
    server_.sendHeader("Cache-Control", "no-cache");
    server_.send(200, "text/html",
                 "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Redirect</title>"
                 "<meta http-equiv='refresh' content='0; url=/'></head><body>"
                 "<p>Redirecting to portal...</p></body></html>");
}

void CWifiMgr::handleNotFound() {
    server_.sendHeader("Location", "/", true);
    server_.send(302, "text/plain", "Redirecting to portal...");
}

// ---------- AP / Portal ----------
void CWifiMgr::startConfigPortal() {
    if (portalActive_) {
        Serial.println("WifiMgr: Portal already active.");
        return;
    }

    Serial.println("WifiMgr: Starting configuration & captive portal AP...");

    // Ensure previous AP is shut down
    WiFi.softAPdisconnect(true);
    delay(50);

    // AP + STA mode so we can still scan if needed
    WiFi.mode(WIFI_MODE_APSTA);
    WiFi.persistent(false);

    wifi_config_t apConfig;
    memset(&apConfig, 0, sizeof(apConfig));

    // Fixed SSID (open network)
    strncpy(reinterpret_cast<char*>(apConfig.ap.ssid), AP_SSID, sizeof(apConfig.ap.ssid) - 1);
    apConfig.ap.ssid_len       = strlen(AP_SSID);
    apConfig.ap.channel        = 1;              // Adjust if you want dynamic channel selection
    apConfig.ap.password[0]    = '\0';           // Open
    apConfig.ap.authmode       = WIFI_AUTH_OPEN;
    apConfig.ap.max_connection = 8;
    apConfig.ap.ssid_hidden    = 0;
    // NOTE: pmf_cfg not present in this SDK version; lines removed.

    esp_err_t err = esp_wifi_set_config(WIFI_IF_AP, &apConfig);
    if (err != ESP_OK) {
        Serial.printf("WifiMgr: esp_wifi_set_config failed: %d\n", (int)err);
    }

    if (!WiFi.softAP(AP_SSID)) {
        Serial.println("WifiMgr: Failed to start AP (softAP).");
    } else {
        Serial.printf("WifiMgr: AP started. SSID: %s (OPEN)\n", AP_SSID);
    }

    IPAddress apIP = WiFi.softAPIP();
    Serial.print("WifiMgr: AP IP address: ");
    Serial.println(apIP);

    WiFi.enableSTA(true);

    if (!dnsServer_.start(DNS_PORT, "*", apIP)) {
        Serial.println("WifiMgr: DNS Server failed to start.");
    } else {
        Serial.println("WifiMgr: DNS Server started for captive portal.");
    }

    server_.on("/",          [this]() { handleRoot(); });
    server_.on("/connect",   HTTP_POST, [this]() { handleConnect(); });
    server_.on("/rescan",    HTTP_GET,  [this]() { handleRescan(); });
    server_.on("/delete",    HTTP_GET,  [this]() { handleDelete(); });

    server_.on("/generate_204",        [this]() { handleCaptive(); });
    server_.on("/gen_204",             [this]() { handleCaptive(); });
    server_.on("/fwlink",              [this]() { handleCaptive(); });
    server_.on("/connecttest.txt",     [this]() { handleCaptive(); });
    server_.on("/hotspot-detect.html", [this]() { handleCaptive(); });
    server_.on("/ncsi.txt",            [this]() { handleCaptive(); });
    server_.on("/favicon.ico",         [this]() { handleCaptive(); });

    server_.onNotFound([this]() { handleNotFound(); });

    server_.begin();
    portalActive_ = true;
    Serial.println("WifiMgr: HTTP portal server started on port 80.");
    String APmsg = "AP mode - Connect to Wi-Fi " + String(AP_SSID) + ", then open http://" + WiFi.softAPIP().toString() + " to configure.";
    if (timeSyncFailed_) {
        FrameBuffer.textScrollAppend(APmsg, CRGB::White, CRGB::Black, 24, 8, true);
        timeSyncFailed_ = false;
    } else {
        FrameBuffer.progressStop();
        FrameBuffer.textScroll(APmsg, 2, "f3x5", CRGB::White, CRGB::Black, 24);
    }
}

void CWifiMgr::stopPortal() {
    if (!portalActive_) return;
    Serial.println("WifiMgr: Stopping configuration portal...");
    server_.stop();
    dnsServer_.stop();
    WiFi.softAPdisconnect(true);
    portalActive_ = false;
    Serial.println("WifiMgr: Portal stopped.");
}