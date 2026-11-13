// Snapshot of the event data kept in RTC memory across a soft reboot: layout and validation
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include "snapshot.h"
#include <stddef.h>
#include <textutil.h>

static uint32_t computeSnapshotChecksum(const EventSnapshot& s) {
    return fnv1a32(reinterpret_cast<const uint8_t *>(&s), offsetof(EventSnapshot, checksum));
}

void sealSnapshot(EventSnapshot& s, const String& fullId) {
    s.magic = SNAPSHOT_MAGIC;
    s.idHash = fnv1a32(fullId);
    s.checksum = computeSnapshotChecksum(s);
}

bool isSnapshotValid(const EventSnapshot& s, const String& fullId) {
    return s.magic == SNAPSHOT_MAGIC && s.checksum == computeSnapshotChecksum(s) && s.idHash == fnv1a32(fullId);
}
