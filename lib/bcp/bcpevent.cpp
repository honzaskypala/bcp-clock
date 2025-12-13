// Best Coast Pairings integration
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include "bcpevent.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

// ---- Event ID related ----

void CBCPEvent::setID(String newID) {
    fullId_ = newID;
    validId_ = false;
    const String prefix = "event/";
    int idx = newID.indexOf(prefix);
    if (idx == -1) {
        id_ = newID;
        return;
    }
    id_ = "";
    for (idx += prefix.length(); idx < newID.length(); idx++) {
        char c = newID[idx];
        if (c == '?' || c == '#' || c == '/') {
            break;
        } else {
            id_ += c;
        }
    }
}

// ---- Refresh data from BCP API ----

bool CBCPEvent::refreshData() {
    if (id_.length() == 0) return false;

    String payload;
    int status = BCPRest("/overview", payload);
    if (status == HTTP_CODE_OK) {
        validId_ = true;
        handleEventOverviewPayload(payload);
        if (started_ && !ended_) {
            fetchTimerData();
        }

    } else if (status == HTTP_CODE_NOT_FOUND) {
        validId_ = false;

    }
    return status == HTTP_CODE_OK || status == HTTP_CODE_NOT_FOUND;
}

void CBCPEvent::handleEventOverviewPayload(const String& payload) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
        int pastRound = currentRound_;

        name_ = doc["name"].as<String>();
        started_ = doc["status"]["started"].as<bool>();
        ended_ = doc["status"]["ended"].as<bool>();
        numberOfRounds_ = doc["status"]["numberOfRounds"].as<int>();
        currentRound_ = doc["status"]["currentRound"].as<int>();
        if (currentRound_ != pastRound) {
            timerLength_ = 0;
            roundStartTime_ = "";
            roundEndTime_ = "";
            roundStartEpoch_ = 0;
            roundEndEpoch_ = 0;
        }
    }
}

void CBCPEvent::fetchTimerData() {
    String payload;
    int status = BCPRest("/timer?round=" + String(currentRound_), payload);
    if (status == HTTP_CODE_OK) {
        handleEventTimerPayload(payload);
    }
}

void CBCPEvent::handleEventTimerPayload(const String& payload) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
        timerLength_ = doc["timerLength"].as<int>();
        roundStartTime_ = doc["startTime"].as<String>();
        struct tm stm = {};
        strptime(roundStartTime_.c_str(), "%Y-%m-%dT%H:%M:%SZ", &stm);
        roundStartEpoch_ = timegm(&stm);
        roundEndTime_ = doc["endTime"].as<String>();
        struct tm etm = {};
        strptime(roundEndTime_.c_str(), "%Y-%m-%dT%H:%M:%SZ", &etm);
        roundEndEpoch_ = timegm(&etm);
        if (doc["timerPaused"].is<bool>()) {
            timerPaused_ = doc["timerPaused"].as<bool>();
            if (timerPaused_) {
                pausedTimeRemaining_ = doc["pausedTimeRemaining"].as<int>();
            }
        } else {
            timerPaused_ = false;
        }
    }
}

int CBCPEvent::BCPRest(const String& endpoint, String& outPayload) {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    if (!http.begin(client, "https://newprod-api.bestcoastpairings.com/v1/events/" + id_ + endpoint)) {
        return -1;
    }
    http.addHeader("client-id", "web-app");

    int status = http.GET();
    if (status == HTTP_CODE_OK) {
        outPayload = http.getString();
    }
    http.end();

    return status;
}

// ---- Helper methods ----

time_t CBCPEvent::timegm(struct tm* t) {
    // remember current TZ
    char* old_tz = getenv("TZ");
    
    // set TZ = UTC
    setenv("TZ", "UTC0", 1);
    tzset();
    
    time_t res = mktime(t);   // treat struct tm as UTC
    
    // restore original TZ
    if (old_tz)
        setenv("TZ", old_tz, 1);
    else
        unsetenv("TZ");
    tzset();

    return res;
}

// ---- Singleton related ----

CBCPEvent::CBCPEvent() {
    // Constructor implementation (if needed)
}

CBCPEvent& getBCPEventInstance() {
    static CBCPEvent instance;
    return instance;
}

CBCPEvent& BCPEvent = getBCPEventInstance();