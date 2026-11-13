// Parsing of the event ID and of the Best Coast Pairings API responses
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include "bcpparse.h"
#include <ArduinoJson.h>
#include <stdlib.h>

String parseEventId(const String& input) {
    const String prefix = "event/";
    int idx = input.indexOf(prefix);
    if (idx == -1) {
        return input;
    }
    String id = "";
    for (idx += prefix.length(); idx < (int) input.length(); idx++) {
        char c = input[idx];
        if (c == '?' || c == '#' || c == '/') {
            break;
        } else {
            id += c;
        }
    }
    return id;
}

template <typename TInput>
static bool parseOverviewFrom(TInput& payload, EventOverview& out) {
    JsonDocument filter;  // the v2 event response is ~6 KB, keep only the fields used
    filter["name"] = true;
    filter["status"] = true;
    JsonDocument doc;
    if (deserializeJson(doc, payload, DeserializationOption::Filter(filter))) {
        return false;
    }
    out.name = doc["name"].as<String>();
    out.started = doc["status"]["started"].as<bool>();
    out.ended = doc["status"]["ended"].as<bool>();
    out.numberOfRounds = doc["status"]["numberOfRounds"].as<int>();
    out.currentRound = doc["status"]["currentRound"].as<int>();
    return true;
}

template <typename TInput>
static bool parseTimerFrom(TInput& payload, TimerData& out) {
    JsonDocument doc;
    if (deserializeJson(doc, payload)) {
        return false;
    }
    out = TimerData();
    if (!doc["timerLength"].is<int>()) {
        return true;  // round without timer: BCP returns {}
    }
    out.timerLength = doc["timerLength"].as<int>();
    out.startTime = doc["startTime"].as<String>();
    out.startEpoch = parseIsoUtc(out.startTime.c_str());
    out.endTime = doc["endTime"].as<String>();
    out.endEpoch = parseIsoUtc(out.endTime.c_str());
    if (doc["timerPaused"].is<bool>()) {
        out.paused = doc["timerPaused"].as<bool>();
        if (out.paused) {
            out.pausedTimeRemaining = doc["pausedTimeRemaining"].as<int>();
        }
    }
    return true;
}

bool parseOverview(const String& payload, EventOverview& out) {
    return parseOverviewFrom(payload, out);
}

bool parseTimer(const String& payload, TimerData& out) {
    return parseTimerFrom(payload, out);
}

#if ARDUINOJSON_ENABLE_ARDUINO_STREAM
bool parseOverview(Stream& payload, EventOverview& out) {
    return parseOverviewFrom(payload, out);
}

bool parseTimer(Stream& payload, TimerData& out) {
    return parseTimerFrom(payload, out);
}
#endif

time_t parseIsoUtc(const char *iso) {
    struct tm t = {};
    strptime(iso, "%Y-%m-%dT%H:%M:%SZ", &t);

    // mktime() works in local time: switch TZ to UTC for the conversion
    const char *oldTzEnv = getenv("TZ");
    String oldTz = oldTzEnv ? String(oldTzEnv) : String();
    setenv("TZ", "UTC0", 1);
    tzset();

    time_t res = mktime(&t);

    if (oldTzEnv) {
        setenv("TZ", oldTz.c_str(), 1);
    } else {
        unsetenv("TZ");
    }
    tzset();
    return res;
}
