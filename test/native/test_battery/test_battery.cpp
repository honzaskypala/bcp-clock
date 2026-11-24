// Unit tests of the battery level (lib/core/battery)
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include <unity.h>
#include <battery.h>

void setUp() {}
void tearDown() {}

// ---- nextBatteryLevel ----

static void test_level_fromUnknown() {
    TEST_ASSERT_EQUAL(BATTERY_OK, nextBatteryLevel(BATTERY_UNKNOWN, 2700));
    TEST_ASSERT_EQUAL(BATTERY_OK, nextBatteryLevel(BATTERY_UNKNOWN, BATTERY_RAW_LOW));
    TEST_ASSERT_EQUAL(BATTERY_LOW, nextBatteryLevel(BATTERY_UNKNOWN, BATTERY_RAW_LOW - 1));
    TEST_ASSERT_EQUAL(BATTERY_LOW, nextBatteryLevel(BATTERY_UNKNOWN, BATTERY_RAW_CRITICAL));
    TEST_ASSERT_EQUAL(BATTERY_CRITICAL, nextBatteryLevel(BATTERY_UNKNOWN, BATTERY_RAW_CRITICAL - 1));
}

static void test_level_worseAppliesAtOnce() {
    TEST_ASSERT_EQUAL(BATTERY_LOW, nextBatteryLevel(BATTERY_OK, BATTERY_RAW_LOW - 1));
    TEST_ASSERT_EQUAL(BATTERY_CRITICAL, nextBatteryLevel(BATTERY_OK, BATTERY_RAW_CRITICAL - 1));
    TEST_ASSERT_EQUAL(BATTERY_CRITICAL, nextBatteryLevel(BATTERY_LOW, BATTERY_RAW_CRITICAL - 1));
}

static void test_level_betterNeedsHysteresis() {
    TEST_ASSERT_EQUAL(BATTERY_LOW, nextBatteryLevel(BATTERY_LOW, BATTERY_RAW_LOW));
    TEST_ASSERT_EQUAL(BATTERY_LOW, nextBatteryLevel(BATTERY_LOW, BATTERY_RAW_LOW + BATTERY_RAW_HYSTERESIS - 1));
    TEST_ASSERT_EQUAL(BATTERY_OK, nextBatteryLevel(BATTERY_LOW, BATTERY_RAW_LOW + BATTERY_RAW_HYSTERESIS));
    TEST_ASSERT_EQUAL(BATTERY_CRITICAL, nextBatteryLevel(BATTERY_CRITICAL, BATTERY_RAW_CRITICAL + BATTERY_RAW_HYSTERESIS - 1));
    TEST_ASSERT_EQUAL(BATTERY_LOW, nextBatteryLevel(BATTERY_CRITICAL, BATTERY_RAW_CRITICAL + BATTERY_RAW_HYSTERESIS));
    TEST_ASSERT_EQUAL(BATTERY_OK, nextBatteryLevel(BATTERY_CRITICAL, 2700));  // charging
}

// ---- BatteryMonitor ----

static void addSamples(BatteryMonitor& m, uint16_t raw, int n) {
    for (int i = 0; i < n; i++) m.addSample(raw);
}

static void test_monitor_unknownUntilWindowFull() {
    BatteryMonitor m;
    addSamples(m, 2270, BatteryMonitor::WINDOW - 1);
    TEST_ASSERT_EQUAL(BATTERY_UNKNOWN, m.level());
    TEST_ASSERT_EQUAL(0, m.filteredRaw());
    m.addSample(2270);
    TEST_ASSERT_EQUAL(BATTERY_CRITICAL, m.level());
    TEST_ASSERT_EQUAL(2270, m.filteredRaw());
}

static void test_monitor_discardsInvalid() {
    BatteryMonitor m;
    addSamples(m, 2450, BatteryMonitor::WINDOW - 1);
    m.addSample(50);
    m.addSample(4095);
    TEST_ASSERT_EQUAL(BATTERY_UNKNOWN, m.level());
    m.addSample(2450);
    TEST_ASSERT_EQUAL(BATTERY_OK, m.level());
}

static void test_monitor_medianIgnoresSpikes() {
    BatteryMonitor m;
    addSamples(m, 2450, BatteryMonitor::WINDOW);
    addSamples(m, 2270, BatteryMonitor::WINDOW / 2);  // short dip under load
    TEST_ASSERT_EQUAL(2450, m.filteredRaw());
    TEST_ASSERT_EQUAL(BATTERY_OK, m.level());
    m.addSample(2270);                                // now the majority
    TEST_ASSERT_EQUAL(2270, m.filteredRaw());
    TEST_ASSERT_EQUAL(BATTERY_CRITICAL, m.level());
}

static void test_monitor_hysteresis() {
    BatteryMonitor m;
    addSamples(m, 2310, BatteryMonitor::WINDOW);     // low
    TEST_ASSERT_EQUAL(BATTERY_LOW, m.level());
    addSamples(m, 2325, BatteryMonitor::WINDOW);     // above the threshold, within the hysteresis
    TEST_ASSERT_EQUAL(BATTERY_LOW, m.level());
    addSamples(m, 2331, BatteryMonitor::WINDOW);     // ok
    TEST_ASSERT_EQUAL(BATTERY_OK, m.level());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_level_fromUnknown);
    RUN_TEST(test_level_worseAppliesAtOnce);
    RUN_TEST(test_level_betterNeedsHysteresis);
    RUN_TEST(test_monitor_unknownUntilWindowFull);
    RUN_TEST(test_monitor_discardsInvalid);
    RUN_TEST(test_monitor_medianIgnoresSpikes);
    RUN_TEST(test_monitor_hysteresis);
    return UNITY_END();
}
