// Parsing of the event ID and of the Best Coast Pairings API responses
// (c) 2025 Honza Skýpala
// WTFPL license applies

#pragma once

#include <Arduino.h>
#include <time.h>

// Event ID from an ID or an event URL (".../event/<id>[/?#...]"); "" if there is none
String parseEventId(const String& input);

struct EventOverview {
    String name;
    bool started = false, ended = false;
    int numberOfRounds = 0, currentRound = 0;
};

struct TimerData {
    uint32_t timerLength = 0;  // 0 = no timer for the round
    String startTime, endTime;
    time_t startEpoch = 0, endEpoch = 0;
    bool paused = false;
    uint32_t pausedTimeRemaining = 0;
};

// Both return false on invalid JSON and leave out unchanged
bool parseOverview(const String& payload, EventOverview& out);
bool parseTimer(const String& payload, TimerData& out);   // {} = round without timer

// "YYYY-MM-DDTHH:MM:SSZ" to epoch, independent of the TZ setting
time_t parseIsoUtc(const char *iso);
