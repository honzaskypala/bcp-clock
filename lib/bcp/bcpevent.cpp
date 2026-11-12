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
    } else {
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
    if (id_.length() == 0) {
        idState_ = ID_INVALID;
    } else {
        idState_ = restoreSnapshot() ? ID_VALID : ID_UNKNOWN;
    }
}

// ---- Refresh data from BCP API ----

bool CBCPEvent::refreshData() {
#ifdef BCP_DEV
    if (fake_) return true;
#endif
    lastStatus_ = 0;
    if (id_.length() == 0) return false;

    String payload;
    int status = BCPRest("/overview", payload);
    lastStatus_ = status;
    if (status == HTTP_CODE_OK) {
        idState_ = ID_VALID;
        handleEventOverviewPayload(payload);
        if (started_ && !ended_) {
            fetchTimerData();
        }
        saveSnapshot();

    } else if (status == HTTP_CODE_NOT_FOUND) {
        idState_ = ID_INVALID;

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
#ifdef BCP_DEV
    if (failNetwork_) return HTTPC_ERROR_CONNECTION_REFUSED;
#endif
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

#ifdef BCP_DEV
// ---- Fake data for testing ----

void CBCPEvent::setFakeData(bool started, int currentRound, int numberOfRounds, uint32_t timerLength,
                            time_t roundEndEpoch, bool paused, uint32_t pausedTimeRemaining) {
    fake_ = true;
    idState_ = ID_VALID;
    name_ = "Fake event";
    started_ = started;
    ended_ = false;
    currentRound_ = currentRound;
    numberOfRounds_ = numberOfRounds;
    timerLength_ = timerLength;
    roundEndEpoch_ = roundEndEpoch;
    roundStartEpoch_ = roundEndEpoch - timerLength;
    roundStartTime_ = "";
    roundEndTime_ = "";
    timerPaused_ = paused;
    pausedTimeRemaining_ = pausedTimeRemaining;
}
#endif

// ---- Snapshot of event data surviving a soft reboot ----
// Kept in RTC slow memory that is not initialized on boot, so it survives ESP.restart()
// but not a power cycle. Magic, checksum and event ID hash reject random content.

namespace {

constexpr uint32_t SNAPSHOT_MAGIC = 0xBC9C10C1;

struct EventSnapshot {
    uint32_t magic;
    uint32_t idHash;
    char name[128];
    bool started, ended;
    int32_t numberOfRounds, currentRound;
    uint32_t timerLength;
    int64_t roundStartEpoch, roundEndEpoch;
    bool timerPaused;
    uint32_t pausedTimeRemaining;
    uint32_t checksum;  // must be the last member
};

RTC_NOINIT_ATTR EventSnapshot snapshot;

uint32_t fnv1a32(const uint8_t *data, size_t len, uint32_t h = 2166136261UL) {
    for (size_t i = 0; i < len; ++i) {
        h ^= data[i];
        h *= 16777619UL;
    }
    return h;
}

uint32_t snapshotChecksum(const EventSnapshot& s) {
    return fnv1a32(reinterpret_cast<const uint8_t *>(&s), offsetof(EventSnapshot, checksum));
}

uint32_t idHash(const String& id) {
    return fnv1a32(reinterpret_cast<const uint8_t *>(id.c_str()), id.length());
}

} // namespace

void CBCPEvent::saveSnapshot() {
    EventSnapshot s = {};  // zeroed, so padding bytes are deterministic for the checksum
    s.magic = SNAPSHOT_MAGIC;
    s.idHash = idHash(fullId_);
    strlcpy(s.name, name_.c_str(), sizeof(s.name));
    s.started = started_;
    s.ended = ended_;
    s.numberOfRounds = numberOfRounds_;
    s.currentRound = currentRound_;
    s.timerLength = timerLength_;
    s.roundStartEpoch = roundStartEpoch_;
    s.roundEndEpoch = roundEndEpoch_;
    s.timerPaused = timerPaused_;
    s.pausedTimeRemaining = pausedTimeRemaining_;
    s.checksum = snapshotChecksum(s);
    snapshot = s;
}

bool CBCPEvent::restoreSnapshot() {
    if (snapshot.magic != SNAPSHOT_MAGIC || snapshot.checksum != snapshotChecksum(snapshot)
            || snapshot.idHash != idHash(fullId_)) {
        return false;
    }
    snapshot.name[sizeof(snapshot.name) - 1] = '\0';
    name_ = snapshot.name;
    started_ = snapshot.started;
    ended_ = snapshot.ended;
    numberOfRounds_ = snapshot.numberOfRounds;
    currentRound_ = snapshot.currentRound;
    timerLength_ = snapshot.timerLength;
    roundStartEpoch_ = snapshot.roundStartEpoch;
    roundEndEpoch_ = snapshot.roundEndEpoch;
    timerPaused_ = snapshot.timerPaused;
    pausedTimeRemaining_ = snapshot.pausedTimeRemaining;
    roundStartTime_ = "";
    roundEndTime_ = "";
    return true;
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