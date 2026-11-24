// Battery level from ADC samples, independent of the hardware
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include "battery.h"

// ---- Battery level ----

static BatteryLevel classifyRaw(int raw) {
    if (raw < BATTERY_RAW_CRITICAL) return BATTERY_CRITICAL;
    if (raw < BATTERY_RAW_LOW) return BATTERY_LOW;
    return BATTERY_OK;
}

BatteryLevel nextBatteryLevel(BatteryLevel current, uint16_t raw) {
    BatteryLevel now = classifyRaw(raw);
    if (current == BATTERY_UNKNOWN || now > current) return now;
    BatteryLevel withHysteresis = classifyRaw((int)raw - BATTERY_RAW_HYSTERESIS);
    return withHysteresis < current ? withHysteresis : current;
}

void BatteryMonitor::addSample(uint16_t raw) {
    if (raw < BATTERY_RAW_MIN_VALID || raw > BATTERY_RAW_MAX_VALID) return;
    samples_[next_] = raw;
    next_ = (next_ + 1) % WINDOW;
    if (count_ < WINDOW) count_++;
    if (count_ == WINDOW) {
        level_ = nextBatteryLevel(level_, filteredRaw());
    }
}

uint16_t BatteryMonitor::filteredRaw() const {
    if (count_ < WINDOW) return 0;
    uint16_t sorted[WINDOW];
    for (int i = 0; i < WINDOW; i++) {
        uint16_t value = samples_[i];
        int j = i;
        for (; j > 0 && sorted[j - 1] > value; j--) {
            sorted[j] = sorted[j - 1];
        }
        sorted[j] = value;
    }
    return sorted[WINDOW / 2];
}
