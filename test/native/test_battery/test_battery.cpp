// Unit tests of the battery level and the low battery alert phase (lib/core/battery)
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include <unity.h>
#include <battery.h>

void setUp() {}
void tearDown() {}

// ---- batteryPercent ----

static void test_percent_linearAndClamped() {
    TEST_ASSERT_EQUAL(0, batteryPercent(1000));
    TEST_ASSERT_EQUAL(0, batteryPercent(BATTERY_RAW_EMPTY));
    TEST_ASSERT_EQUAL(4, batteryPercent(1937));
    TEST_ASSERT_EQUAL(5, batteryPercent(1938));
    TEST_ASSERT_EQUAL(9, batteryPercent(1975));
    TEST_ASSERT_EQUAL(10, batteryPercent(1976));
    TEST_ASSERT_EQUAL(50, batteryPercent(2280));
    TEST_ASSERT_EQUAL(100, batteryPercent(BATTERY_RAW_FULL));
    TEST_ASSERT_EQUAL(100, batteryPercent(3500));
}

// ---- nextBatteryLevel ----

static void test_level_fromUnknown() {
    TEST_ASSERT_EQUAL(BATTERY_OK, nextBatteryLevel(BATTERY_UNKNOWN, 10));
    TEST_ASSERT_EQUAL(BATTERY_LOW, nextBatteryLevel(BATTERY_UNKNOWN, 9));
    TEST_ASSERT_EQUAL(BATTERY_LOW, nextBatteryLevel(BATTERY_UNKNOWN, 5));
    TEST_ASSERT_EQUAL(BATTERY_CRITICAL, nextBatteryLevel(BATTERY_UNKNOWN, 4));
}

static void test_level_worseAppliesAtOnce() {
    TEST_ASSERT_EQUAL(BATTERY_LOW, nextBatteryLevel(BATTERY_OK, 9));
    TEST_ASSERT_EQUAL(BATTERY_CRITICAL, nextBatteryLevel(BATTERY_OK, 4));
    TEST_ASSERT_EQUAL(BATTERY_CRITICAL, nextBatteryLevel(BATTERY_LOW, 4));
}

static void test_level_betterNeedsHysteresis() {
    TEST_ASSERT_EQUAL(BATTERY_LOW, nextBatteryLevel(BATTERY_LOW, 10));
    TEST_ASSERT_EQUAL(BATTERY_LOW, nextBatteryLevel(BATTERY_LOW, 11));
    TEST_ASSERT_EQUAL(BATTERY_OK, nextBatteryLevel(BATTERY_LOW, 12));
    TEST_ASSERT_EQUAL(BATTERY_CRITICAL, nextBatteryLevel(BATTERY_CRITICAL, 6));
    TEST_ASSERT_EQUAL(BATTERY_LOW, nextBatteryLevel(BATTERY_CRITICAL, 7));
    TEST_ASSERT_EQUAL(BATTERY_OK, nextBatteryLevel(BATTERY_CRITICAL, 50));  // charging
}

// ---- BatteryMonitor ----

static void addSamples(BatteryMonitor& m, uint16_t raw, int n) {
    for (int i = 0; i < n; i++) m.addSample(raw);
}

static void test_monitor_unknownUntilWindowFull() {
    BatteryMonitor m;
    addSamples(m, 1920, BatteryMonitor::WINDOW - 1);
    TEST_ASSERT_EQUAL(BATTERY_UNKNOWN, m.level());
    TEST_ASSERT_EQUAL(0, m.filteredRaw());
    m.addSample(1920);
    TEST_ASSERT_EQUAL(BATTERY_CRITICAL, m.level());
    TEST_ASSERT_EQUAL(1920, m.filteredRaw());
}

static void test_monitor_discardsInvalid() {
    BatteryMonitor m;
    addSamples(m, 2400, BatteryMonitor::WINDOW - 1);
    m.addSample(50);
    m.addSample(4095);
    TEST_ASSERT_EQUAL(BATTERY_UNKNOWN, m.level());
    m.addSample(2400);
    TEST_ASSERT_EQUAL(BATTERY_OK, m.level());
}

static void test_monitor_medianIgnoresSpikes() {
    BatteryMonitor m;
    addSamples(m, 2400, BatteryMonitor::WINDOW);
    addSamples(m, 1920, BatteryMonitor::WINDOW / 2);  // short dip under load
    TEST_ASSERT_EQUAL(2400, m.filteredRaw());
    TEST_ASSERT_EQUAL(BATTERY_OK, m.level());
    m.addSample(1920);                                // now the majority
    TEST_ASSERT_EQUAL(1920, m.filteredRaw());
    TEST_ASSERT_EQUAL(BATTERY_CRITICAL, m.level());
}

static void test_monitor_hysteresis() {
    BatteryMonitor m;
    addSamples(m, 1960, BatteryMonitor::WINDOW);     // 7 % -> low
    TEST_ASSERT_EQUAL(BATTERY_LOW, m.level());
    addSamples(m, 1984, BatteryMonitor::WINDOW);     // 11 % -> still low
    TEST_ASSERT_EQUAL(BATTERY_LOW, m.level());
    addSamples(m, 1992, BatteryMonitor::WINDOW);     // 12 % -> ok
    TEST_ASSERT_EQUAL(BATTERY_OK, m.level());
}

// ---- isBatteryAlertPhase ----

static void test_alertPhase_alternatesWhenCritical() {
    TEST_ASSERT_FALSE(isBatteryAlertPhase(BATTERY_CRITICAL, 0, DISPLAY_EVENT_COUNTDOWN));
    TEST_ASSERT_FALSE(isBatteryAlertPhase(BATTERY_CRITICAL, 4999, DISPLAY_EVENT_COUNTDOWN));
    TEST_ASSERT_TRUE(isBatteryAlertPhase(BATTERY_CRITICAL, 5000, DISPLAY_EVENT_COUNTDOWN));
    TEST_ASSERT_TRUE(isBatteryAlertPhase(BATTERY_CRITICAL, 9999, DISPLAY_EVENT_ROUND));
    TEST_ASSERT_FALSE(isBatteryAlertPhase(BATTERY_CRITICAL, 10000, DISPLAY_EVENT_NAME));
    TEST_ASSERT_TRUE(isBatteryAlertPhase(BATTERY_CRITICAL, 15000, DISPLAY_BOOT));
}

static void test_alertPhase_onlyWhenCritical() {
    TEST_ASSERT_FALSE(isBatteryAlertPhase(BATTERY_UNKNOWN, 5000, DISPLAY_EVENT_COUNTDOWN));
    TEST_ASSERT_FALSE(isBatteryAlertPhase(BATTERY_OK, 5000, DISPLAY_EVENT_COUNTDOWN));
    TEST_ASSERT_FALSE(isBatteryAlertPhase(BATTERY_LOW, 5000, DISPLAY_EVENT_COUNTDOWN));
}

static void test_alertPhase_notDuringConfigMessage() {
    TEST_ASSERT_FALSE(isBatteryAlertPhase(BATTERY_CRITICAL, 5000, DISPLAY_CONFIG_MESSAGE));
    TEST_ASSERT_TRUE(isBatteryAlertPhase(BATTERY_CRITICAL, 5000, DISPLAY_CONFIG_REQUIRED));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_percent_linearAndClamped);
    RUN_TEST(test_level_fromUnknown);
    RUN_TEST(test_level_worseAppliesAtOnce);
    RUN_TEST(test_level_betterNeedsHysteresis);
    RUN_TEST(test_monitor_unknownUntilWindowFull);
    RUN_TEST(test_monitor_discardsInvalid);
    RUN_TEST(test_monitor_medianIgnoresSpikes);
    RUN_TEST(test_monitor_hysteresis);
    RUN_TEST(test_alertPhase_alternatesWhenCritical);
    RUN_TEST(test_alertPhase_onlyWhenCritical);
    RUN_TEST(test_alertPhase_notDuringConfigMessage);
    return UNITY_END();
}
