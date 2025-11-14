#ifndef BCPEVENT_H
#define BCPEVENT_H

#include <Arduino.h>

class CBCPEvent {
public:
    void setID(String newID);
    String getID() const { return id; }
    void refreshData();

    String getName() const { return name; }
    bool isStarted() const { return started; }
    bool isEnded() const { return ended; }
    int getNumberOfRounds() const { return numberOfRounds; }
    int getCurrentRound() const { return currentRound; }
    int getTimerLength() const { return timerLength; }
    String getRoundStartTime() const { return roundStartTime; }
    String getRoundEndTime() const { return roundEndTime; }
    time_t getRoundStartEpoch() const { return roundStartEpoch; }
    time_t getRoundEndEpoch() const { return roundEndEpoch; }

private:
    CBCPEvent();
    CBCPEvent(const CBCPEvent&) = delete;
    CBCPEvent& operator=(const CBCPEvent&) = delete;

    friend CBCPEvent& getBCPEventInstance();

    String id;
    String extractEventID(const String& input);

    String name = "";
    bool started = false, ended = false;
    int numberOfRounds = 0, currentRound = 0;
    int timerLength = 0;
    String roundStartTime = "", roundEndTime = "";
    time_t roundStartEpoch = 0, roundEndEpoch = 0;
    time_t timegm(struct tm* t);
};

extern CBCPEvent& BCPEvent;

#endif