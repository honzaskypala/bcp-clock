// String helpers: time thresholds, hostname, stored WiFi list, hashing
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include "textutil.h"
#include <ctype.h>

static const char *DEF_HOSTNAME = "BCP-clock";
static constexpr unsigned int MAX_HOSTNAME_LEN = 32;   // ESP-IDF netif limit

// ---- Time thresholds ----

String secondsToTime(long seconds) {
    long m = seconds / 60;
    long s = seconds % 60;
    char buf[16];
    snprintf(buf, sizeof(buf), "%ld:%02ld", m, s);
    return String(buf);
}

long timeToSeconds(const String& time) {
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

// ---- Hostname ----

String sanitizeHostname(const String& name) {
    String out;
    for (unsigned int i = 0; i < name.length() && out.length() < MAX_HOSTNAME_LEN; i++) {
        char c = name[i];
        if (isalnum(static_cast<unsigned char>(c)) || (c == '-' && out.length() > 0)) {
            out += c;
        }
    }
    while (out.endsWith("-")) {
        out.remove(out.length() - 1);
    }
    return out.length() > 0 ? out : String(DEF_HOSTNAME);
}

// ---- Newline separated list ----

void splitList(const String& src, std::vector<String>& out) {
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

String joinList(const std::vector<String>& list) {
    String s;
    for (size_t i = 0; i < list.size(); ++i) {
        s += list[i];
        if (i + 1 < list.size()) s += '\n';
    }
    return s;
}

// ---- Hashing ----

uint32_t fnv1a32(const uint8_t *data, size_t len, uint32_t h) {
    for (size_t i = 0; i < len; ++i) {
        h ^= data[i];
        h *= 16777619UL;
    }
    return h;
}

uint32_t fnv1a32(const String& s) {
    return fnv1a32(reinterpret_cast<const uint8_t *>(s.c_str()), s.length());
}

String makePassKey(const String& ssid) {
    char buf[13]; // "pwd_" + 8 hex + null
    snprintf(buf, sizeof(buf), "pwd_%08x", (unsigned)fnv1a32(ssid));
    return String(buf);
}
