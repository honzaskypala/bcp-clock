#include "bcpevent.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

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

void CBCPEvent::refreshData() {
    if (this->id.length() == 0) return;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = "https://newprod-api.bestcoastpairings.com/v1/events/" + this->id + "/overview";

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
            this->name = doc["name"].as<String>();
            this->started = doc["status"]["started"].as<bool>();
            this->ended = doc["status"]["ended"].as<bool>();
            this->numberOfRounds = doc["status"]["numberOfRounds"].as<int>();
            this->currentRound = doc["status"]["currentRound"].as<int>();
        }
    }
    
    http.end();
}