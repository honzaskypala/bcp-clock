// On-device tests of the platform dependent parts: newlib time conversion, JSON parsing, config in NVS
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include <Arduino.h>
#include <unity.h>
#include <Preferences.h>
#include <bcpparse.h>
#include <config.h>

void setUp() {}
void tearDown() {}

static const char *TEST_NS = "cfgtest";   // never the real "config" namespace

// ---- Time (newlib strptime / mktime differ from the host libc) ----

static void test_isoUtc_newlib() {
    unsetenv("TZ");
    tzset();
    TEST_ASSERT_EQUAL_INT32(1763456400, (int32_t) parseIsoUtc("2025-11-18T09:00:00Z"));
    TEST_ASSERT_EQUAL_INT32(1767225600, (int32_t) parseIsoUtc("2026-01-01T00:00:00Z"));
    TEST_ASSERT_EQUAL_INT32(1709164800, (int32_t) parseIsoUtc("2024-02-29T00:00:00Z"));
    TEST_ASSERT_NULL(getenv("TZ"));
}

static void test_isoUtc_newlib_withTz() {
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();
    TEST_ASSERT_EQUAL_INT32(1751365800, (int32_t) parseIsoUtc("2025-07-01T10:30:00Z"));
    TEST_ASSERT_EQUAL_STRING("CET-1CEST,M3.5.0,M10.5.0/3", getenv("TZ"));
    unsetenv("TZ");
    tzset();
}

static void test_timerPayload_onDevice() {
    TimerData t;
    TEST_ASSERT_TRUE(parseTimer("{\"timerLength\":9000,\"startTime\":\"2025-11-18T09:00:00Z\","
                                "\"endTime\":\"2025-11-18T11:30:00Z\",\"timerPaused\":true,"
                                "\"pausedTimeRemaining\":1234}", t));
    TEST_ASSERT_EQUAL_INT32(1763456400 + 9000, (int32_t) t.endEpoch);
    TEST_ASSERT_TRUE(t.paused);
    TEST_ASSERT_EQUAL_UINT32(1234, t.pausedTimeRemaining);
}

// ---- Config in NVS ----

static ConfigValues readTestNs() {
    Preferences prefs;
    TEST_ASSERT_TRUE(prefs.begin(TEST_NS, true));
    ConfigValues v = readConfig(prefs);
    prefs.end();
    return v;
}

static void clearTestNs() {
    Preferences prefs;
    TEST_ASSERT_TRUE(prefs.begin(TEST_NS, false));
    prefs.clear();
    prefs.end();
}

static void test_config_defaults() {
    clearTestNs();
    ConfigValues v = readTestNs();
    TEST_ASSERT_EQUAL_STRING("", v.event.c_str());
    TEST_ASSERT_EQUAL(600, v.yellow);
    TEST_ASSERT_EQUAL(0, v.red);
    TEST_ASSERT_EQUAL_STRING("BCP-clock", v.hostname.c_str());
}

static void test_config_longValues() {
    clearTestNs();
    Preferences prefs;
    prefs.begin(TEST_NS, false);
    prefs.putString("event", "AbC123");
    prefs.putLong("yellow", 300);
    prefs.putLong("red", 60);
    prefs.putString("hostname", "table-1");
    prefs.end();
    ConfigValues v = readTestNs();
    TEST_ASSERT_EQUAL_STRING("AbC123", v.event.c_str());
    TEST_ASSERT_EQUAL(300, v.yellow);
    TEST_ASSERT_EQUAL(60, v.red);
    TEST_ASSERT_EQUAL_STRING("table-1", v.hostname.c_str());
}

static void test_config_legacyStringThresholds() {
    clearTestNs();
    Preferences prefs;
    prefs.begin(TEST_NS, false);
    prefs.putString("yellow", "5:00");
    prefs.putString("red", "0:30");
    prefs.end();
    ConfigValues v = readTestNs();
    TEST_ASSERT_EQUAL(300, v.yellow);
    TEST_ASSERT_EQUAL(30, v.red);
}

static void test_config_hostnameSanitizedOnLoad() {
    clearTestNs();
    Preferences prefs;
    prefs.begin(TEST_NS, false);
    prefs.putString("hostname", "-my clock_1-");
    prefs.end();
    TEST_ASSERT_EQUAL_STRING("myclock1", readTestNs().hostname.c_str());
    clearTestNs();
}

void setup() {
    delay(2000);   // the serial port reset the board, give the test runner time to attach
    UNITY_BEGIN();
    RUN_TEST(test_isoUtc_newlib);
    RUN_TEST(test_isoUtc_newlib_withTz);
    RUN_TEST(test_timerPayload_onDevice);
    RUN_TEST(test_config_defaults);
    RUN_TEST(test_config_longValues);
    RUN_TEST(test_config_legacyStringThresholds);
    RUN_TEST(test_config_hostnameSanitizedOnLoad);
    UNITY_END();
}

void loop() {}
