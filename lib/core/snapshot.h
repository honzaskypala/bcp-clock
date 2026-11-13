// Snapshot of the event data kept in RTC memory across a soft reboot: layout and validation
// (c) 2025 Honza Skýpala
// WTFPL license applies

#pragma once

#include <Arduino.h>

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

// Sets magic, event ID hash and checksum; the snapshot must be zero-initialized before
// filling the data, so padding bytes are deterministic for the checksum
void snapshotSeal(EventSnapshot& s, const String& fullId);

// Magic, checksum and event ID hash reject random content and another event
bool snapshotValid(const EventSnapshot& s, const String& fullId);
