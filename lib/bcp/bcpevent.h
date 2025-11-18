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
    String id() { return id_; }
    String fullId() { return fullId_; }
    bool valid() { return validId_; }

    // ---- Refresh data from BCP API ----
    bool refreshData();

    // ---- BCP event details ----
    String name() const { return name_; }
    bool started() const { return started_; }
    bool ended() const { return ended_; }
    int numberOfRounds() const { return numberOfRounds_; }
    int currentRound() const { return currentRound_; }

    // ---- BCP round timer details ----
    int timerLength() const { return timerLength_; }
    String roundStartTime() const { return roundStartTime_; }
    String roundEndTime() const { return roundEndTime_; }
    time_t roundStartEpoch() const { return roundStartEpoch_; }
    time_t roundEndEpoch() const { return roundEndEpoch_; }

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

    // ---- BCP event details ----
    String name_ = "";
    bool started_ = false, ended_ = false;
    int numberOfRounds_ = 0, currentRound_ = 0;

    // ---- BCP round timer details ----
    int timerLength_ = 0;
    String roundStartTime_ = "", roundEndTime_ = "";
    time_t roundStartEpoch_ = 0, roundEndEpoch_ = 0;

    // ---- Helper methods ----
    time_t timegm(struct tm* t);
};

// ---- Singleton instance ----
extern CBCPEvent& BCPEvent;

#endif