// On-device tests of the platform dependent parts: newlib time conversion, JSON parsing (also from a Stream), config in NVS
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

// ---- JSON parsed from a Stream (the HTTP response path) ----

// Read-only in-memory Stream; ends like a closed connection (read() returns -1)
class MemStream : public Stream {
public:
    explicit MemStream(const char *data) : data_(data), pos_(0), len_(strlen(data)) { setTimeout(0); }
    int available() override { return len_ - pos_; }
    int read() override { return pos_ < len_ ? (uint8_t) data_[pos_++] : -1; }
    int peek() override { return pos_ < len_ ? (uint8_t) data_[pos_] : -1; }
    size_t write(uint8_t) override { return 0; }
private:
    const char *data_;
    size_t pos_, len_;
};

static void test_overviewStream_v2() {
    MemStream in("{\"id\":\"AbC123\",\"owner\":{\"id\":\"o1\",\"name\":\"Owner\"},"
                 "\"name\":\"Prague Open – 2025\",\"gameSystem\":{\"name\":\"X\"},"
                 "\"status\":{\"started\":true,\"ended\":false,\"currentRound\":3,\"numberOfRounds\":5},"
                 "\"teamEventOptions\":{}}");
    EventOverview o;
    TEST_ASSERT_TRUE(parseOverview(in, o));
    TEST_ASSERT_EQUAL_STRING("Prague Open – 2025", o.name.c_str());
    TEST_ASSERT_TRUE(o.started);
    TEST_ASSERT_FALSE(o.ended);
    TEST_ASSERT_EQUAL(5, o.numberOfRounds);
    TEST_ASSERT_EQUAL(3, o.currentRound);
}

static void test_overviewStream_truncated_unchanged() {
    MemStream in("{\"name\":\"X\",\"status\":{\"started\":tr");
    EventOverview o;
    o.name = "keep";
    TEST_ASSERT_FALSE(parseOverview(in, o));
    TEST_ASSERT_EQUAL_STRING("keep", o.name.c_str());
}

static void test_timerStream() {
    MemStream in("{\"timerLength\":3600,\"startTime\":\"2025-11-18T09:00:00Z\","
                 "\"endTime\":\"2025-11-18T10:00:00Z\",\"timerPaused\":false}");
    TimerData t;
    TEST_ASSERT_TRUE(parseTimer(in, t));
    TEST_ASSERT_EQUAL_UINT32(3600, t.timerLength);
    TEST_ASSERT_EQUAL_INT32(1763456400 + 3600, (int32_t) t.endEpoch);
    TEST_ASSERT_FALSE(t.paused);
}

static void test_timerStream_empty() {
    MemStream in("{}");
    TimerData t;
    t.timerLength = 99;
    TEST_ASSERT_TRUE(parseTimer(in, t));
    TEST_ASSERT_EQUAL_UINT32(0, t.timerLength);
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
    pinMode(15, INPUT_PULLDOWN);   // silence the TC001 buzzer (GPIO 15) before anything else
    delay(2000);   // the serial port reset the board, give the test runner time to attach
    UNITY_BEGIN();
    RUN_TEST(test_isoUtc_newlib);
    RUN_TEST(test_isoUtc_newlib_withTz);
    RUN_TEST(test_timerPayload_onDevice);
    RUN_TEST(test_overviewStream_v2);
    RUN_TEST(test_overviewStream_truncated_unchanged);
    RUN_TEST(test_timerStream);
    RUN_TEST(test_timerStream_empty);
    RUN_TEST(test_config_defaults);
    RUN_TEST(test_config_longValues);
    RUN_TEST(test_config_legacyStringThresholds);
    RUN_TEST(test_config_hostnameSanitizedOnLoad);
    UNITY_END();
}

void loop() {}
