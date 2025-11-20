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

    const String baseUrl = "https://newprod-api.bestcoastpairings.com/v1/events/" + id_;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = baseUrl + "/overview";
    if (!http.begin(client, url)) {
        return false;
    }
    http.addHeader("client-id", "web-app");
    int status = http.GET();
    if (status == HTTP_CODE_OK) {
        validId_ = true;
        String payload = http.getString();
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

            if (started_ && !ended_) {
                HTTPClient timerHttp;
                String timerUrl = baseUrl + "/timer?round=" + String(currentRound_);
                if (!timerHttp.begin(client, timerUrl)) {
                    return status == HTTP_CODE_OK;
                }
                timerHttp.addHeader("client-id", "web-app");
                int timerStatus = timerHttp.GET();
                if (timerStatus == HTTP_CODE_OK) {
                    String timerPayload = timerHttp.getString();
                    JsonDocument timerDoc;
                    DeserializationError timerError = deserializeJson(timerDoc, timerPayload);
                    if (!timerError) {
                        timerLength_ = timerDoc["timerLength"].as<int>();
                        roundStartTime_ = timerDoc["startTime"].as<String>();
                        struct tm stm = {};
                        strptime(roundStartTime_.c_str(), "%Y-%m-%dT%H:%M:%SZ", &stm);
                        roundStartEpoch_ = timegm(&stm);
                        roundEndTime_ = timerDoc["endTime"].as<String>();
                        struct tm etm = {};
                        strptime(roundEndTime_.c_str(), "%Y-%m-%dT%H:%M:%SZ", &etm);
                        roundEndEpoch_ = timegm(&etm);
                        if (timerDoc.containsKey("timerPaused")) {
                            timerPaused_ = timerDoc["timerPaused"].as<bool>();
                            if (timerPaused_) {
                                pausedTimeRemaining_ = timerDoc["pausedTimeRemaining"].as<int>();
                            }
                        } else {
                            timerPaused_ = false;
                        }
                    }
                }
                timerHttp.end();
            }
        }

    } else if (status == HTTP_CODE_NOT_FOUND) {
        validId_ = false;

    }
    http.end();
    return status == HTTP_CODE_OK || status == HTTP_CODE_NOT_FOUND;
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