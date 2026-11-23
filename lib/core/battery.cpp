// Battery level from ADC samples and the low battery alert phase, independent of the hardware
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include "battery.h"

// ---- Battery level ----

int batteryPercent(uint16_t raw) {
    if (raw <= BATTERY_RAW_EMPTY) return 0;
    if (raw >= BATTERY_RAW_FULL) return 100;
    return (int)(raw - BATTERY_RAW_EMPTY) * 100 / (BATTERY_RAW_FULL - BATTERY_RAW_EMPTY);
}

static BatteryLevel classifyPercent(int percent) {
    if (percent < BATTERY_CRITICAL_PERCENT) return BATTERY_CRITICAL;
    if (percent < BATTERY_LOW_PERCENT) return BATTERY_LOW;
    return BATTERY_OK;
}

BatteryLevel nextBatteryLevel(BatteryLevel current, int percent) {
    BatteryLevel now = classifyPercent(percent);
    if (current == BATTERY_UNKNOWN || now > current) return now;
    BatteryLevel withHysteresis = classifyPercent(percent - BATTERY_HYSTERESIS_PERCENT);
    return withHysteresis < current ? withHysteresis : current;
}

void BatteryMonitor::addSample(uint16_t raw) {
    if (raw < BATTERY_RAW_MIN_VALID || raw > BATTERY_RAW_MAX_VALID) return;
    samples_[next_] = raw;
    next_ = (next_ + 1) % WINDOW;
    if (count_ < WINDOW) count_++;
    if (count_ == WINDOW) {
        level_ = nextBatteryLevel(level_, batteryPercent(filteredRaw()));
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

// ---- Critical battery alert ----

bool isBatteryAlertPhase(BatteryLevel level, unsigned long ms, DisplayState state) {
    return level == BATTERY_CRITICAL && state != DISPLAY_CONFIG_MESSAGE && (ms / BATTERY_ALERT_PHASE_MS) % 2 == 1;
}
