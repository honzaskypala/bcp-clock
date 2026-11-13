// Snapshot of the event data kept in RTC memory across a soft reboot: layout and validation
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include "snapshot.h"
#include <stddef.h>
#include <textutil.h>

static uint32_t snapshotChecksum(const EventSnapshot& s) {
    return fnv1a32(reinterpret_cast<const uint8_t *>(&s), offsetof(EventSnapshot, checksum));
}

void snapshotSeal(EventSnapshot& s, const String& fullId) {
    s.magic = SNAPSHOT_MAGIC;
    s.idHash = fnv1a32(fullId);
    s.checksum = snapshotChecksum(s);
}

bool snapshotValid(const EventSnapshot& s, const String& fullId) {
    return s.magic == SNAPSHOT_MAGIC && s.checksum == snapshotChecksum(s) && s.idHash == fnv1a32(fullId);
}
