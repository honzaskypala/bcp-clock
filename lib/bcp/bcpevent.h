#ifndef BCPEVENT_H
#define BCPEVENT_H

#include <Arduino.h>

class CBCPEvent {
public:
    void setID(String newID);
    String getID() const { return id; }
    void refreshData();

    String name = "";
    bool started = false, ended = false;
    int numberOfRounds = 0, currentRound = 0;

private:
    CBCPEvent();
    CBCPEvent(const CBCPEvent&) = delete;
    CBCPEvent& operator=(const CBCPEvent&) = delete;

    friend CBCPEvent& getBCPEventInstance();

    String id;
    String extractEventID(const String& input);
};

extern CBCPEvent& BCPEvent;






#endif