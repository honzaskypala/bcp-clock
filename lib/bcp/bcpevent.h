// Best Coast Pairings integration
// (c) 2025 Honza Skýpala
// WTFPL license applies

#ifndef BCPEVENT_H
#define BCPEVENT_H

#include <Arduino.h>

class CBCPEvent {
public:
    // ---- Event ID related ----
    void setID(String newID);
    inline String id() { return id_; }
    inline String fullId() { return fullId_; }
    inline bool valid() { return idState_ == ID_VALID; }
    inline bool invalid() { return idState_ == ID_INVALID; }

    // ---- Refresh data from BCP API ----
    bool refreshData();
    inline int lastStatus() const { return lastStatus_; }  // last HTTP status, <0 = connection error, 0 = no request

    // ---- BCP event details ----
    inline String name() const { return name_; }
    inline bool started() const { return started_; }
    inline bool ended() const { return ended_; }
    inline int numberOfRounds() const { return numberOfRounds_; }
    inline int currentRound() const { return currentRound_; }

    // ---- BCP round timer details ----
    inline uint32_t timerLength() const { return timerLength_; }
    inline String roundStartTime() const { return roundStartTime_; }
    inline String roundEndTime() const { return roundEndTime_; }
    inline time_t roundStartEpoch() const { return roundStartEpoch_; }
    inline time_t roundEndEpoch() const { return roundEndEpoch_; }
    inline bool timerPaused() const { return timerPaused_; }
    inline uint32_t pausedTimeRemaining() const { return pausedTimeRemaining_; }

#ifdef BCP_DEV
    // ---- Fake data for testing (dev build only) ----
    // Replaces event data; refreshData() does not contact BCP until clearFakeData()
    void setFakeData(bool started, int currentRound, int numberOfRounds, uint32_t timerLength,
                     time_t roundEndEpoch, bool paused, uint32_t pausedTimeRemaining);
    void clearFakeData() { fake_ = false; }
    inline bool fake() const { return fake_; }
    // Simulates connection errors (BCPRest returns -1)
    void setFailNetwork(bool fail) { failNetwork_ = fail; }
    inline bool failNetwork() const { return failNetwork_; }
private:
    bool fake_ = false;
    bool failNetwork_ = false;
#endif

private:
    // ---- Singleton related ----
    CBCPEvent();
    CBCPEvent(const CBCPEvent&) = delete;
    CBCPEvent& operator=(const CBCPEvent&) = delete;
    friend CBCPEvent& getBCPEventInstance();

    // ---- Event ID related ----
    enum IdState { ID_UNKNOWN, ID_VALID, ID_INVALID };
    IdState idState_ = ID_UNKNOWN;
    String id_;
    String fullId_;

    // ---- REST API ----
    int BCPRest(const String& path, String& outPayload);  // path after the host, e.g. /v2/events/<id>
    int lastStatus_ = 0;

    // ---- BCP event details ----
    String name_ = "";
    bool started_ = false, ended_ = false;
    int numberOfRounds_ = 0, currentRound_ = 0;
    void handleEventOverviewPayload(const String& payload);

    // ---- BCP round timer details ----
    uint32_t timerLength_ = 0;
    String roundStartTime_ = "", roundEndTime_ = "";
    time_t roundStartEpoch_ = 0, roundEndEpoch_ = 0;
    bool timerPaused_ = false;
    uint32_t pausedTimeRemaining_ = 0;
    void fetchTimerData();
    void clearTimerData();
    void handleEventTimerPayload(const String& payload);

    // ---- Snapshot of event data surviving a soft reboot ----
    void saveSnapshot();
    bool restoreSnapshot();
};

// ---- Singleton instance ----
extern CBCPEvent& BCPEvent;

#endif