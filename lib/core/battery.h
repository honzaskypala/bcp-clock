// Battery level from ADC samples, independent of the hardware
// (c) 2025 Honza Skýpala
// WTFPL license applies

#pragma once

#include <stdint.h>

// ---- Battery level ----

// Raw 12-bit ADC values of the TC001 battery voltage (GPIO 34, default 11 dB attenuation). AWTRIX3 uses
// 475 (empty) and 665 (full), which match 10-bit readings (a TC001 charging on USB reads ~2710 here);
// these are the same values x4. To be calibrated.
constexpr uint16_t BATTERY_RAW_EMPTY = 1900;      // 0 %
constexpr uint16_t BATTERY_RAW_FULL = 2660;       // 100 %
constexpr uint16_t BATTERY_RAW_MIN_VALID = 400;   // readings outside 400..4000 are discarded
constexpr uint16_t BATTERY_RAW_MAX_VALID = 4000;

constexpr int BATTERY_LOW_PERCENT = 10;           // below: blinking status indicator
constexpr int BATTERY_CRITICAL_PERCENT = 5;       // below: battery icon alternates with the content
constexpr int BATTERY_HYSTERESIS_PERCENT = 2;     // a level is left only this much above its threshold

// Ordered from good to bad (except unknown)
enum BatteryLevel {
    BATTERY_UNKNOWN,   // not enough samples yet
    BATTERY_OK,
    BATTERY_LOW,
    BATTERY_CRITICAL
};

int batteryPercent(uint16_t raw);  // linear between empty and full, clamped to 0..100

// Level after a new percent reading: a worse level applies at once, a better one only with the hysteresis
BatteryLevel nextBatteryLevel(BatteryLevel current, int percent);

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
