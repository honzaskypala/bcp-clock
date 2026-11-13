// ESP32 WiFi Manager with captive portal
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include "wifimgr.h"

#include <WiFi.h>
#include <time.h>
#include <esp_wifi.h>
#include <vector>
#include <RTClib.h>
#include <webui.h>
#include <textutil.h>

// ---------- Config ----------
static const uint32_t CONNECT_TIMEOUT_MS = 15000;
static const uint32_t PORTAL_CONNECT_TIMEOUT_MS = 20000;
static constexpr const char* AP_SSID = "BCP-clock";
static const char* DEFAULT_AP_PASSWORD = "";
static const byte DNS_PORT = 53;
static const char* NTP_SERVER = "pool.ntp.org";
static const long GMT_OFFSET_SEC = 0;
static const int DAYLIGHT_OFFSET_SEC = 0;     // UTC everywhere, the countdown is epoch-based
static const uint32_t MIN_PLAUSIBLE_EPOCH = 1763487357;   // 2025-11-18, older RTC time is considered invalid
static constexpr const char* PREFS_NS       = "wificreds";
static constexpr const char* PREFS_LIST_KEY = "__ssids";
constexpr const char* CONNECTION_FAIL_MSG = "Error: Connection failed";
constexpr const char* NTP_TIME_SYNC_FAIL_MSG = "Error: NTP time sync failed";

#define WIFIMGR_DEBUG(msg) if (debugOut_) { debugOut_->print("[WiFiMgr] "); debugOut_->println(msg); }
#define WIFIMGR_MSG(msg) if (usrMsg_) { usrMsg_->println(msg); }

// -----------Singleton----------
CWifiMgr WifiMgr;

// ---------- Public API ----------
bool CWifiMgr::connect(bool enforcePortal, uint32_t portalTimeoutMs, Print *usrMsg, Print *debugOut) {
    usrMsg_ = usrMsg;
    debugOut_ = debugOut;

    WIFIMGR_DEBUG("Attempting automatic WiFi connection.");
    connected_ = false;
    timeSyncFailed_ = false;

    if (!scanAndSort()) {
        WIFIMGR_DEBUG("Scan failed or found no networks.");

    } else if (!enforcePortal) {
        if (tryKnownNetworks() || tryOpenNetworks()) {
            connected_ = true;
            freeScanData();
            return true;
        }
        if (timeSyncFailed_) {
            WIFIMGR_MSG(NTP_TIME_SYNC_FAIL_MSG);
        }
    }

    if (portalTimeoutMs > 0) {
        std::vector<String> storedSsids;
        if (!listStoredNetworks(storedSsids)) {
            WIFIMGR_DEBUG("No stored networks to retry, portal will run with no timeout.");
            portalTimeoutMs = 0;
        }
    }

    WIFIMGR_DEBUG("Launching captive configuration portal (blocking until connected or timeout).");
    startConfigPortal();

    while (!connected_) {
        dnsServer_.processNextRequest();
        server_.handleClient();
        delay(10);
        if (portalTimeoutMs > 0 && (millis() - lastPortalActivityMs_) > portalTimeoutMs) {
            WIFIMGR_DEBUG("Portal timeout reached, exiting without WiFi.");
            break;
        }
    }

    freeScanData();
    return connected_;
}

// Pass 1: known networks (DO NOT save credential here)
bool CWifiMgr::tryKnownNetworks() {
    std::vector<String> storedSsids;
    if (!listStoredNetworks(storedSsids)) {
        WIFIMGR_DEBUG("No known networks (missing or invalid /wifi.json).");
        return false;
    }

    WIFIMGR_DEBUG("Pass 1 - Trying known networks...");
    for (int order = 0; order < scanCount_; ++order) {
        int i = scanIdx_[order];
        const String& ssid = scanSsid_[i];
        WIFIMGR_DEBUG("Considering SSID: " + ssid);
        if (ssid.length() == 0) continue;

        String pass;
        if (getCredential(ssid.c_str(), pass)) {
            WIFIMGR_DEBUG("Known SSID candidate: " + ssid + " (RSSI " + String(scanRssi_[order]) + " dBm)");
            if (attemptConnect(ssid.c_str(), pass.c_str(), CONNECT_TIMEOUT_MS)) {
                WIFIMGR_DEBUG("Connected via known network (no credential save).");
                return true;
            }
        }
        delay(0);
    }
    WIFIMGR_DEBUG("Pass 1 finished: No known networks connected.");
    return false;
}

// Pass 2: open networks (DO NOT save credential here)
bool CWifiMgr::tryOpenNetworks() {
    WIFIMGR_DEBUG("Pass 2 - Trying open networks...");
    for (int order = 0; order < scanCount_; ++order) {
        int i = scanIdx_[order];
        if (!isOpenEncryption(scanEnc_[i])) continue;
        const String& ssid = scanSsid_[i];
        if (ssid.length() == 0) continue;

        WIFIMGR_DEBUG("Open SSID candidate: " + ssid + " (RSSI " + String(scanRssi_[order]) + " dBm)");
        if (attemptConnect(ssid.c_str(), nullptr, CONNECT_TIMEOUT_MS)) {
            WIFIMGR_DEBUG("Connected via open network (no credential save).");
            return true;
        }
        delay(0);
    }
    WIFIMGR_DEBUG("Pass 2 finished: No open networks connected.");
    return false;
}

bool CWifiMgr::eraseStoredNetworks() {
    WIFIMGR_DEBUG("Removing all stored SSIDs and credentials.");
    if (!beginPrefs(false)) {
        WIFIMGR_DEBUG("Preferences begin (RW) failed.");
        return false;
    }

    String listStr = readStoredList();

    std::vector<String> list;
    splitList(listStr, list);

    for (auto &s : list) {
        String key = makePassKey(s);
        if (prefs_.isKey(key.c_str())) {
            prefs_.remove(key.c_str());
        }
    }

    if (prefs_.isKey(PREFS_LIST_KEY)) {
        prefs_.remove(PREFS_LIST_KEY);
    }

    prefs_.end();
    WIFIMGR_DEBUG("Completed.");
    return true;
}

// ---------- Core steps ----------
bool CWifiMgr::scanAndSort() {
    ensureStaModeForScan();

    WIFIMGR_DEBUG("Preparing for WiFi scan...");
    if (!portalActive_) {
        WIFIMGR_DEBUG("Waiting 3 seconds before scan to avoid early boot scan issues...");
        delay(3000);
    } else {
        WIFIMGR_DEBUG("Portal active - skipping initial scan delay to keep AP responsive.");
        delay(50);
    }

    WIFIMGR_DEBUG("Starting WiFi scan...");
    int n = WiFi.scanNetworks();
    WIFIMGR_DEBUG("Scan done");

    if (n <= 0) {
        WIFIMGR_DEBUG("No networks found");
        return false;
    }

    if (!loadDriverScanResults(n)) {
        loadFallbackScanResults(n);
    }
    WiFi.scanDelete();
    sortScanByRssi();

    WIFIMGR_DEBUG("" + String(scanCount_) + " networks found (sorted strongest to weakest)");
    return true;
}

void CWifiMgr::allocScanArrays(int count) {
    scanCount_ = count;
    scanIdx_   = new int[scanCount_];
    scanRssi_  = new int[scanCount_];
    scanSsid_  = new String[scanCount_];
    scanEnc_   = new uint8_t[scanCount_];
    scanChan_  = new int[scanCount_];
}

// Reads the scan results with the driver API (includes BSSID), returns false if they are not available
bool CWifiMgr::loadDriverScanResults(int n) {
    uint16_t count = static_cast<uint16_t>(n);
    wifi_ap_record_t* recs = static_cast<wifi_ap_record_t*>(malloc(count * sizeof(wifi_ap_record_t)));
    if (!recs) return false;
    if (esp_wifi_scan_get_ap_records(&count, recs) != ESP_OK || count == 0) {
        free(recs);
        return false;
    }

    allocScanArrays(static_cast<int>(count));
    WIFIMGR_DEBUG("Scan results (driver):");
    for (int i = 0; i < scanCount_; ++i) {
        scanIdx_[i]  = i;
        scanSsid_[i] = String(reinterpret_cast<const char*>(recs[i].ssid));
        scanRssi_[i] = static_cast<int>(recs[i].rssi);
        scanEnc_[i]  = static_cast<uint8_t>(recs[i].authmode);
        scanChan_[i] = static_cast<int>(recs[i].primary);

        const uint8_t* b = recs[i].bssid;
        WIFIMGR_DEBUG("  [" + String(i) + "] SSID='" + scanSsid_[i] + "' RSSI=" + String(scanRssi_[i]) + " dBm ENC=" + String((unsigned)scanEnc_[i]) + " CH=" + String(scanChan_[i]) + " BSSID=" + String(b[0], HEX) + ":" + String(b[1], HEX) + ":" + String(b[2], HEX) + ":" + String(b[3], HEX) + ":" + String(b[4], HEX) + ":" + String(b[5], HEX));
    }
    free(recs);
    return true;
}

void CWifiMgr::loadFallbackScanResults(int n) {
    allocScanArrays(n);
    WIFIMGR_DEBUG("Scan results (fallback API):");
    for (int i = 0; i < scanCount_; ++i) {
        scanIdx_[i]  = i;
        scanSsid_[i] = WiFi.SSID(i);
        scanRssi_[i] = WiFi.RSSI(i);
        scanEnc_[i]  = (uint8_t)WiFi.encryptionType(i);
        scanChan_[i] = WiFi.channel(i);
        WIFIMGR_DEBUG("  [" + String(i) + "] SSID='" + scanSsid_[i] + "' RSSI=" + String(scanRssi_[i]) + " dBm ENC=" + String((unsigned)scanEnc_[i]) + " CH=" + String(scanChan_[i]));
    }
}

// Sort by RSSI descending (keep idx indirection)
void CWifiMgr::sortScanByRssi() {
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

    WIFIMGR_DEBUG("Trying to connect to SSID: " + String(ssid));
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
        WIFIMGR_DEBUG("Connected. IP address: " + WiFi.localIP().toString());
        if (!syncTime(10000)) {
            WIFIMGR_DEBUG("NTP sync failed. Disconnecting and failing connect attempt.");
            WiFi.disconnect(true, true);
            timeSyncFailed_ = true;
            return false;
        }

        return true;
    }
    WIFIMGR_DEBUG("Connection attempt failed.");
    return false;
}

// ---------- Credentials load/save/remove ----------
bool CWifiMgr::beginPrefs(bool readOnly) {
    // Return true on success, false on failure
    return prefs_.begin(PREFS_NS, readOnly);
}

// Newline-separated list of stored SSIDs; read only if the key exists, to avoid the NOT_FOUND log
String CWifiMgr::readStoredList() {
    return prefs_.isKey(PREFS_LIST_KEY) ? prefs_.getString(PREFS_LIST_KEY, "") : String();
}

bool CWifiMgr::listStoredNetworks(std::vector<String>& out) {
    out.clear();
    if (!beginPrefs(true)) {
        WIFIMGR_DEBUG("Preferences begin (RO) failed.");
        return false;
    }

    String list = readStoredList();
    prefs_.end();

    splitList(list, out);
    if (!out.empty()) {
        WIFIMGR_DEBUG("Loaded stored SSIDs from Preferences:");
        for (auto& s : out) WIFIMGR_DEBUG("  SSID: " + s);
        return true;
    }
    WIFIMGR_DEBUG("No stored SSIDs in Preferences.");
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
        WIFIMGR_DEBUG("Empty SSID, skipping.");
        return false;
    }
    if (!beginPrefs(false)) {
        WIFIMGR_DEBUG("Preferences begin (RW) failed.");
        return false;
    }

    String listStr = readStoredList();

    std::vector<String> list;
    splitList(listStr, list);
    bool found = false;
    for (auto& s : list) if (s == ssid) { found = true; break; }
    if (!found) list.push_back(String(ssid));
    prefs_.putString(PREFS_LIST_KEY, joinList(list));

    String key = makePassKey(String(ssid));
    prefs_.putString(key.c_str(), (pass ? pass : ""));

    prefs_.end();
    WIFIMGR_DEBUG("Stored credential for '" + String(ssid));
    return true;
}

bool CWifiMgr::removeStoredNetwork(const char* ssid) {
    if (!ssid || ssid[0] == '\0') {
        WIFIMGR_DEBUG("Empty SSID, skipping.");
        return false;
    }
    if (!beginPrefs(false)) {
        WIFIMGR_DEBUG("Preferences begin (RW) failed.");
        return false;
    }

    String listStr = readStoredList();

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
        WIFIMGR_DEBUG("SSID '" + String(ssid) + "' not found.");
        return false;
    }
    prefs_.putString(PREFS_LIST_KEY, joinList(out));

    String key = makePassKey(String(ssid));
    if (prefs_.isKey(key.c_str())) {
        prefs_.remove(key.c_str());
    }

    prefs_.end();
    WIFIMGR_DEBUG("Removed '" + String(ssid) + "'.");
    return true;
}

// ---- Prepare for Wi-Fi scan ----
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

// ---- Time sync ----

bool CWifiMgr::syncTime(uint32_t timeoutMs) {
    WIFIMGR_DEBUG("Syncing time via NTP...");
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);

    RTC_DS1307 rtc;
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, timeoutMs)) {
        WIFIMGR_DEBUG("Time synchronized:");
        printLocalTime();
        if (rtc.begin()) {
            rtc.adjust(DateTime((uint32_t) time(nullptr)));  // Update RTC time
        }
        return true;
    }
    if (!rtc.begin() || rtc.now().unixtime() <= MIN_PLAUSIBLE_EPOCH) {
        WIFIMGR_DEBUG("Failed to obtain time.");
        return false;
    }
    WIFIMGR_DEBUG("NTP sync failed, but RTC has valid time. Using RTC time.");
    struct timeval tv;
    tv.tv_sec = rtc.now().unixtime();
    tv.tv_usec = 0;
    settimeofday(&tv, nullptr);
    printLocalTime();
    return true;
}

void CWifiMgr::printLocalTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        WIFIMGR_DEBUG("Failed to obtain time.");
        return;
    }
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %Z", &timeinfo);
    WIFIMGR_DEBUG("Local time: " + String(buf));
}

// ---------- HTML helpers ----------
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
#ifdef BCP_DEV
// Logs portal requests, e.g. to see which captive portal detection URLs a phone requests
#define WIFIMGR_LOG_REQUEST(handler) WIFIMGR_DEBUG(String("Portal request [") + handler + "] " + \
        (server_.method() == HTTP_GET ? "GET " : "other ") + server_.hostHeader() + server_.uri() + \
        " UA: " + server_.header("User-Agent"))
#else
#define WIFIMGR_LOG_REQUEST(handler)
#endif

void CWifiMgr::handleRoot() {
    WIFIMGR_LOG_REQUEST("root");
    lastPortalActivityMs_ = millis();
    bool showFail = server_.hasArg("fail");

    String page = WebUI::buildPageStart("BCP-clock Wi-Fi Setup",
            "label.ssid{display:flex;align-items:center;gap:10px;font-size:22px;margin:10px 0}"
            "input[type=radio]{width:20px;height:20px}"
            ".rssi{color:#666;font-size:12px}"
            "@keyframes spin{to{transform:rotate(360deg)}}"
            ".waitIcon{display:none}"
            ".spinning .waitIcon,.spinning .icon{display:inline-block;animation:spin .8s linear infinite}");
    page.reserve(10000);

    if (showFail) {
        page += "<div class='status error'>" + String(timeSyncFailed_ ? NTP_TIME_SYNC_FAIL_MSG : CONNECTION_FAIL_MSG) + "</div>";
    }

    page += "<form method='POST' action='/connect'"
            " onsubmit=\"var b=this.querySelector('.btnMain'); if(b){b.classList.add('spinning'); b.disabled=true;} return true;\">";
    page += WebUI::buildSectionHeader("Found networks",
            "<button class='btn' type='button' onclick='onRescanClick(this)'><span class='icon'>↻</span><span>Rescan</span></button>",
            true);

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
            page += "<input type='radio' name='ssid' value='" + WebUI::escapeHtml(ssid) + "'" + (order == 0 ? " checked" : "") + ">";
            page += "<span>" + WebUI::escapeHtml(ssid) + "</span>";
            page += "<span class='rssi'>(" + String(sec) + ", " + String(rssi) + " dBm)</span>";
            page += "</label>";
        }
        page += "<div class='row'><label for='pwd'>Password:</label>"
                "<input id='pwd' type='password' name='pwd' placeholder='Leave empty for OPEN network'></div>";
        page += "<div class='actions'>"
                "<button class='btn btnMain' type='submit'><span>Connect</span><span class='waitIcon' aria-hidden='true'>⏳</span></button>"
                "</div>";
    }
    page += "</form>";
    page += "<script>"
            "function onRescanClick(btn){"
                "if(btn.classList.contains('spinning')) return;"
                "btn.classList.add('spinning');"
                "btn.disabled=true;"
                "void btn.offsetWidth;"
                "requestAnimationFrame(function(){setTimeout(function(){window.location.href='/rescan';},100);});"
            "}"
            "</script>";

    page += buildStoredWifisHtml();
    page += WebUI::buildPageEnd();
    server_.send(200, "text/html", page);
}

String CWifiMgr::buildStoredWifisHtml() {
    std::vector<String> storedSsids;
    bool haveStoredNetworks = listStoredNetworks(storedSsids) && !storedSsids.empty();

    String page = WebUI::buildSectionHeader("Stored Wi-Fi networks", haveStoredNetworks ?
            "<a class='btn btnDanger' href='#' onclick='delAll();return false;'>Delete all 👉🗑️</a>" : "");
    if (haveStoredNetworks) {
        page += "<div class='storedList'>";
        for (auto& s : storedSsids) {
            page += "<div class='storedRow'>"
                    "<span class='storedSSID'>" + WebUI::escapeHtml(s) + "</span>"
                    "<a class='del' href='#' data-ssid='" + WebUI::escapeHtml(s) + "' title='Delete'"
                    " onclick='delNet(this.dataset.ssid);return false;'>👉🗑️</a>"
                    "</div>";
        }
        page += "</div>";
    } else {
        page += "<p>No networks stored.</p>";
    }
    // Deletion uses the DELETE method: a cross-site page cannot send it without a CORS preflight,
    // which the device does not answer
    page += "<script>"
                "function delReq(url,msg,next){"
                    "if(!confirm(msg))return;"
                    "fetch(url,{method:'DELETE'})"
                        ".then(function(r){return r.text().then(function(t){if(r.ok)location.href=next;else alert(t);});})"
                        ".catch(function(e){alert(e);});"
                "}"
                "function delNet(s){delReq('/delete?ssid='+encodeURIComponent(s),'Delete SSID '+s+'?','/?wifiDeleted=1');}"
                "function delAll(){delReq('/deleteAll','Delete ALL stored SSIDs and passwords?','/?wifiAllDeleted=1');}"
            "</script>";
    return page;
}

void CWifiMgr::handleRescan() {
    WIFIMGR_DEBUG("Portal - Rescanning networks...");
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

    WIFIMGR_DEBUG("Portal - User selected SSID: " + ssid);
    WIFIMGR_DEBUG("Portal - Attempting connection...");
    WIFIMGR_MSG("Connecting to '" + ssid + "'...");

    timeSyncFailed_ = false;
    if (attemptConnect(ssid.c_str(), pwd.c_str(), PORTAL_CONNECT_TIMEOUT_MS)) {
        connected_ = true;
        saveCredential(ssid.c_str(), pwd.c_str());

        String resp = WebUI::buildPageStart("BCP-clock Wi-Fi Setup");
        resp += "<div class='status ok'>Connected to " + WebUI::escapeHtml(ssid) + "</div>";
        resp += "<p>IP address: " + WiFi.localIP().toString() + "</p>";
        resp += "<p>Network saved. The setup portal shuts down now.</p>";
        resp += WebUI::buildPageEnd();
        server_.send(200, "text/html", resp);
        delay(500);
        stopPortal();
        WIFIMGR_MSG("Connected to '" + ssid + "'...");
    } else {
        // Redirect back to root with fail flag so root shows status bar
        server_.sendHeader("Location", "/?fail=1", true);
        String failMsg = timeSyncFailed_ ? NTP_TIME_SYNC_FAIL_MSG : CONNECTION_FAIL_MSG;
        server_.send(302, "text/plain", failMsg);
        WIFIMGR_DEBUG(failMsg);
        WIFIMGR_MSG(failMsg);
    }
}

void CWifiMgr::handleDelete() {
    if (!server_.hasArg("ssid")) {
        server_.send(400, "text/plain", "Missing ssid parameter");
        return;
    }
    String ssid = server_.arg("ssid");
    WIFIMGR_DEBUG("Portal - Request to delete SSID '" + ssid + "'.");

    bool ok = removeStoredNetwork(ssid.c_str());
    if (ok) {
        server_.send(200, "text/plain", "Deleted");
    } else {
        server_.send(404, "text/plain", "Delete failed or SSID not found");
    }
}

void CWifiMgr::handleDeleteAll() {
    WIFIMGR_DEBUG("Portal - Request to delete ALL stored SSIDs.");
    bool ok = eraseStoredNetworks();
    if (ok) {
        server_.send(200, "text/plain", "All deleted");
    } else {
        server_.send(500, "text/plain", "Delete all failed");
    }
}

void CWifiMgr::handleCaptive() {
    WIFIMGR_LOG_REQUEST("captive");
    server_.sendHeader("Cache-Control", "no-cache");
    server_.send(200, "text/html",
                 "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Redirect</title>"
                 "<meta http-equiv='refresh' content='0; url=/'></head><body>"
                 "<p>Redirecting to portal...</p></body></html>");
}

void CWifiMgr::handleNotFound() {
    WIFIMGR_LOG_REQUEST("notFound");
    server_.sendHeader("Location", "/", true);
    server_.send(302, "text/plain", "Redirecting to portal...");
}

// ---------- AP / Portal ----------
void CWifiMgr::startConfigPortal() {
    if (portalActive_) {
        WIFIMGR_DEBUG("Portal already active.");
        return;
    }

    WIFIMGR_DEBUG("Starting configuration & captive portal AP...");

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
        WIFIMGR_DEBUG("esp_wifi_set_config failed: " + String((int)err));
    }

    if (!WiFi.softAP(AP_SSID)) {
        WIFIMGR_DEBUG("Failed to start AP (softAP).");
    } else {
        WIFIMGR_DEBUG("AP started. SSID: " + String(AP_SSID) + " (OPEN)");
    }

    IPAddress apIP = WiFi.softAPIP();
    WIFIMGR_DEBUG("AP IP address: " + apIP.toString());

    WiFi.enableSTA(true);

    if (!dnsServer_.start(DNS_PORT, "*", apIP)) {
        WIFIMGR_DEBUG("DNS Server failed to start.");
    } else {
        WIFIMGR_DEBUG("DNS Server started for captive portal.");
    }

    server_.on("/",          [this]() { handleRoot(); });
    server_.on("/connect",   HTTP_POST, [this]() { handleConnect(); });
    server_.on("/rescan",    HTTP_GET,  [this]() { handleRescan(); });
    server_.on("/delete",    HTTP_DELETE, [this]() { handleDelete(); });
    server_.on("/deleteAll", HTTP_DELETE, [this]() { handleDeleteAll(); });

    server_.on("/generate_204",        [this]() { handleCaptive(); });
    server_.on("/gen_204",             [this]() { handleCaptive(); });
    server_.on("/fwlink",              [this]() { handleCaptive(); });
    server_.on("/connecttest.txt",     [this]() { handleCaptive(); });
    server_.on("/hotspot-detect.html", [this]() { handleCaptive(); });
    server_.on("/ncsi.txt",            [this]() { handleCaptive(); });
    server_.on("/favicon.ico",         [this]() { handleCaptive(); });

    server_.onNotFound([this]() { handleNotFound(); });
#ifdef BCP_DEV
    const char *logHeaders[] = {"User-Agent"};
    server_.collectHeaders(logHeaders, 1);
#endif

    server_.begin();
    portalActive_ = true;
    lastPortalActivityMs_ = millis();
    WIFIMGR_DEBUG("HTTP portal server started on port 80.");
    WIFIMGR_MSG("AP mode - Connect to Wi-Fi '" + String(AP_SSID) + "', then open http://" + WiFi.softAPIP().toString() + " to configure.");
}

void CWifiMgr::stopPortal() {
    if (!portalActive_) return;
    WIFIMGR_DEBUG("Stopping configuration portal...");
    server_.stop();
    dnsServer_.stop();
    WiFi.softAPdisconnect(true);
    portalActive_ = false;
    WIFIMGR_DEBUG("Portal stopped.");
}