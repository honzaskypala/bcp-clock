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
    inline bool valid() { return validId_; }

    // ---- Refresh data from BCP API ----
    bool refreshData();

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

private:
    // ---- Singleton related ----
    CBCPEvent();
    CBCPEvent(const CBCPEvent&) = delete;
    CBCPEvent& operator=(const CBCPEvent&) = delete;
    friend CBCPEvent& getBCPEventInstance();

    // ---- Event ID related ----
    bool validId_ = false;
    String id_;
    String fullId_;
    String extractEventID(const String& input);

    // ---- REST API ----
    int BCPRest(const String& endpoint, String& outPayload);

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
    void handleEventTimerPayload(const String& payload);

    // ---- Helper methods ----
    time_t timegm(struct tm* t);
};

// ---- Singleton instance ----
extern CBCPEvent& BCPEvent;

#endif