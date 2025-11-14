#include "bcpevent.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

CBCPEvent::CBCPEvent() {
    // Constructor implementation (if needed)
}

CBCPEvent& getBCPEventInstance() {
    static CBCPEvent instance;
    return instance;
}

CBCPEvent& BCPEvent = getBCPEventInstance();

void CBCPEvent::setID(String newID) {
    const String prefix = "event/";
    int idx = newID.indexOf(prefix);
    if (idx == -1) {
        this->id = newID;
        return;
    }
    this->id = "";
    for (idx += prefix.length(); idx < newID.length(); idx++) {
        char c = newID[idx];
        if (c == '?' || c == '#' || c == '/') {
            break;
        } else {
            this->id += c;
        }
    }
}

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

void CBCPEvent::refreshData() {
    if (this->id.length() == 0) return;

    const String baseUrl = "https://newprod-api.bestcoastpairings.com/v1/events/" + this->id;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = baseUrl + "/overview";
    if (!http.begin(client, url)) {
        return;
    }
    http.addHeader("client-id", "web-app");
    int status = http.GET();
    if (status == HTTP_CODE_OK) {
        String payload = http.getString();
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);
        if (!error) {
            int pastRound = this->currentRound;

            this->name = doc["name"].as<String>();
            this->started = doc["status"]["started"].as<bool>();
            this->ended = doc["status"]["ended"].as<bool>();
            this->numberOfRounds = doc["status"]["numberOfRounds"].as<int>();
            this->currentRound = doc["status"]["currentRound"].as<int>();

            if (this->currentRound != pastRound) {
                this->timerLength = 0;
                this->roundStartTime = "";
                this->roundEndTime = "";
                this->roundStartEpoch = 0;
                this->roundEndEpoch = 0;
            }

            if (this->started && !this->ended) {
                HTTPClient timerHttp;
                String timerUrl = baseUrl + "/timer?round=" + String(this->currentRound);
                if (!timerHttp.begin(client, timerUrl)) {
                    return;
                }
                timerHttp.addHeader("client-id", "web-app");
                int timerStatus = timerHttp.GET();
                if (timerStatus == HTTP_CODE_OK) {
                    String timerPayload = timerHttp.getString();
                    JsonDocument timerDoc;
                    DeserializationError timerError = deserializeJson(timerDoc, timerPayload);
                    if (!timerError) {
                        this->timerLength = timerDoc["timerLength"].as<int>();
                        this->roundStartTime = timerDoc["startTime"].as<String>();
                        struct tm stm = {};
                        strptime(this->roundStartTime.c_str(), "%Y-%m-%dT%H:%M:%SZ", &stm);
                        this->roundStartEpoch = this->timegm(&stm);
                        this->roundEndTime = timerDoc["endTime"].as<String>();
                        struct tm etm = {};
                        strptime(this->roundEndTime.c_str(), "%Y-%m-%dT%H:%M:%SZ", &etm);
                        this->roundEndEpoch = this->timegm(&etm);
                    }
                }
                timerHttp.end();
            }
        }
    }
    http.end();
}