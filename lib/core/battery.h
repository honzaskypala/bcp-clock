// Battery level from ADC samples, independent of the hardware
// (c) 2025 Honza Skýpala
// WTFPL license applies

#pragma once

#include <stdint.h>

// ---- Battery level ----

// Raw 12-bit ADC values (median) of the TC001 battery voltage (GPIO 34, default 11 dB attenuation).
// Calibrated by one discharge at full brightness with a white countdown (2026-11-24): 2514 right after
// unplugging, switched off at 2256 after 112 minutes; ~2700 while charging on USB. The thresholds are the
// time left before the switch-off; the voltage depends on how many LEDs are lit, so it is +-5 minutes.
constexpr uint16_t BATTERY_RAW_LOW = 2321;        // below: blinking status indicator (~30 min left)
constexpr uint16_t BATTERY_RAW_CRITICAL = 2288;   // below: battery icon alternates with the content (~10 min left)
constexpr uint16_t BATTERY_RAW_HYSTERESIS = 10;   // a level is left only this much above its threshold
constexpr uint16_t BATTERY_RAW_MIN_VALID = 400;   // readings outside 400..4000 are discarded
constexpr uint16_t BATTERY_RAW_MAX_VALID = 4000;

// Ordered from good to bad (except unknown)
enum BatteryLevel {
    BATTERY_UNKNOWN,   // not enough samples yet
    BATTERY_OK,
    BATTERY_LOW,
    BATTERY_CRITICAL
};

// Level after a new (median) reading: a worse level applies at once, a better one only with the hysteresis
BatteryLevel nextBatteryLevel(BatteryLevel current, uint16_t raw);

// Median of the last samples (removes ADC spikes and voltage dips under load) and the level
class BatteryMonitor {
public:
    static constexpr int WINDOW = 15;

    void addSample(uint16_t raw);         // invalid readings are discarded
    uint16_t filteredRaw() const;         // median of the window, 0 = window not full yet
    BatteryLevel level() const { return level_; }

private:
    uint16_t samples_[WINDOW] = {};
    int count_ = 0;                       // valid samples taken, saturates at WINDOW
    int next_ = 0;                        // ring buffer position of the next sample
    BatteryLevel level_ = BATTERY_UNKNOWN;
};
