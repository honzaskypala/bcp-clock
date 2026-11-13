// Best Coast Pairings integration
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include "bcpevent.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <bcpparse.h>
#include <snapshot.h>

// ---- Event ID related ----

void CBCPEvent::setID(const String& newID) {
    fullId_ = newID;
    id_ = parseEventId(newID);
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

    int status = requestBcp("/v2/events/" + id_, &CBCPEvent::handleEventOverviewPayload);
    lastStatus_ = status;
    if (status == HTTP_CODE_OK) {
        idState_ = ID_VALID;
        if (started_ && !ended_) {
            fetchTimerData();
        }
        saveSnapshot();

    } else if (status == HTTP_CODE_NOT_FOUND) {
        idState_ = ID_INVALID;

    }
    return status == HTTP_CODE_OK || status == HTTP_CODE_NOT_FOUND;
}

void CBCPEvent::handleEventOverviewPayload(Stream& payload) {
    EventOverview overview;
    if (parseOverview(payload, overview)) {
        int pastRound = currentRound_;

        name_ = overview.name;
        started_ = overview.started;
        ended_ = overview.ended;
        numberOfRounds_ = overview.numberOfRounds;
        currentRound_ = overview.currentRound;
        if (currentRound_ != pastRound) {
            clearTimerData();
        }
    }
}

void CBCPEvent::fetchTimerData() {
    int status = requestBcp("/v1/events/" + id_ + "/timer?round=" + String(currentRound_),
                         &CBCPEvent::handleEventTimerPayload);
    if (status == HTTP_CODE_NOT_FOUND) {
        clearTimerData();  // BCP returns 200 {} for no timer now; 404 handled in case that changes
    }
    // other errors (connection, 5xx): keep the last timer data
}

void CBCPEvent::clearTimerData() {
    timerLength_ = 0;
    roundStartTime_ = "";
    roundEndTime_ = "";
    roundStartEpoch_ = 0;
    roundEndEpoch_ = 0;
    timerPaused_ = false;
    pausedTimeRemaining_ = 0;
}

void CBCPEvent::handleEventTimerPayload(Stream& payload) {
    TimerData timer;
    if (parseTimer(payload, timer)) {   // {} = round without timer, clears the data
        timerLength_ = timer.timerLength;
        roundStartTime_ = timer.startTime;
        roundEndTime_ = timer.endTime;
        roundStartEpoch_ = timer.startEpoch;
        roundEndEpoch_ = timer.endEpoch;
        timerPaused_ = timer.paused;
        pausedTimeRemaining_ = timer.pausedTimeRemaining;
    }
}

int CBCPEvent::requestBcp(const String& path, void (CBCPEvent::*onBody)(Stream&)) {
#ifdef BCP_DEV
    if (failNetwork_) return HTTPC_ERROR_CONNECTION_REFUSED;
#endif
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    if (!http.begin(client, "https://newprod-api.bestcoastpairings.com" + path)) {
        return -1;
    }
    http.addHeader("client-id", "web-app");
    http.useHTTP10(true);  // no chunked transfer encoding, so the body can be parsed from the raw stream

    int status = http.GET();
    if (status == HTTP_CODE_OK) {
        (this->*onBody)(http.getStream());
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
// but not a power cycle. Layout and validation in lib/core/snapshot.

RTC_NOINIT_ATTR static EventSnapshot snapshot;

void CBCPEvent::saveSnapshot() {
    EventSnapshot s = {};  // zeroed, so padding bytes are deterministic for the checksum
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
    sealSnapshot(s, fullId_);
    snapshot = s;
}

bool CBCPEvent::restoreSnapshot() {
    if (!isSnapshotValid(snapshot, fullId_)) {
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

// ---- Singleton related ----

CBCPEvent::CBCPEvent() {
    // Constructor implementation (if needed)
}

CBCPEvent& getBCPEventInstance() {
    static CBCPEvent instance;
    return instance;
}

CBCPEvent& BCPEvent = getBCPEventInstance();