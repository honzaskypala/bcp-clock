// Operating modes of the clock: identifiers stored in NVS, order of switching
// (c) 2025 Honza Skýpala
// WTFPL license applies

#pragma once

#include <stdint.h>

// Stored in NVS (config/mode), keep the values when adding a mode
enum ModeId : uint8_t {
    MODE_BCP = 0,
    MODE_OFFLINE = 1,
    MODE_CHESS = 2
};

// Index of the stored mode in the modes of the build, 0 (the first mode) if the build does not contain it
inline int findModeIndex(const ModeId ids[], int count, uint8_t storedId) {
    for (int i = 0; i < count; i++) {
        if (ids[i] == storedId) return i;
    }
    return 0;
}

// The mode after index, wrapping around; the same index with a single mode
inline int nextModeIndex(int index, int count) {
    return count <= 0 ? 0 : (index + 1) % count;
}
