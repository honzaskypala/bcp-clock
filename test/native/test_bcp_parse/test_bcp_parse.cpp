// Unit tests of the event ID and BCP API response parsing (lib/core/bcpparse)
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include <unity.h>
#include <Arduino.h>
#include <bcpparse.h>
#include <stdlib.h>

void setUp() {}
void tearDown() {}

// ---- Event ID ----

static void test_eventId_plainId() {
    TEST_ASSERT_EQUAL_STRING("AbC123xyz", parseEventId("AbC123xyz").c_str());
}

static void test_eventId_fromUrl() {
    TEST_ASSERT_EQUAL_STRING("AbC123", parseEventId("https://www.bestcoastpairings.com/event/AbC123").c_str());
    TEST_ASSERT_EQUAL_STRING("AbC123", parseEventId("https://www.bestcoastpairings.com/event/AbC123?active_tab=pairings").c_str());
    TEST_ASSERT_EQUAL_STRING("AbC123", parseEventId("https://www.bestcoastpairings.com/event/AbC123#x").c_str());
    TEST_ASSERT_EQUAL_STRING("AbC123", parseEventId("https://www.bestcoastpairings.com/event/AbC123/pairings").c_str());
    TEST_ASSERT_EQUAL_STRING("AbC123", parseEventId("bestcoastpairings.com/event/AbC123").c_str());
}

static void test_eventId_empty() {
    TEST_ASSERT_EQUAL_STRING("", parseEventId("").c_str());
    TEST_ASSERT_EQUAL_STRING("", parseEventId("https://www.bestcoastpairings.com/event/").c_str());
    TEST_ASSERT_EQUAL_STRING("", parseEventId("https://www.bestcoastpairings.com/event/?x=1").c_str());
}

// Documents current behaviour: surrounding whitespace is kept (no trim)
static void test_eventId_whitespaceKept() {
    TEST_ASSERT_EQUAL_STRING(" AbC123 ", parseEventId(" AbC123 ").c_str());
}

// ---- event overview (/v2/events/<id>) ----

static const char *OVERVIEW_RUNNING =
    "{\"id\":\"AbC123\",\"name\":\"Prague Open – 2025\",\"status\":{\"started\":true,\"ended\":false,"
    "\"numberOfRounds\":5,\"currentRound\":3},\"extra\":[1,2,3]}";

static void test_overview_running() {
    EventOverview o;
    TEST_ASSERT_TRUE(parseOverview(OVERVIEW_RUNNING, o));
    TEST_ASSERT_EQUAL_STRING("Prague Open – 2025", o.name.c_str());
    TEST_ASSERT_TRUE(o.started);
    TEST_ASSERT_FALSE(o.ended);
    TEST_ASSERT_EQUAL(5, o.numberOfRounds);
    TEST_ASSERT_EQUAL(3, o.currentRound);
}

// v2 shape: "name" also nested in other objects, large sections around "status"
static const char *OVERVIEW_V2 =
    "{\"id\":\"AbC123\",\"owner\":{\"id\":\"o1\",\"name\":\"Owner\"},"
    "\"metrics\":{\"placings\":[{\"isOn\":true,\"name\":\"Tournament Points\",\"key\":\"tp\"}]},"
    "\"name\":\"Cobras con la fresca\","
    "\"gameSystem\":{\"id\":\"g1\",\"name\":\"Warhammer Underworlds\"},"
    "\"status\":{\"started\":true,\"ended\":true,\"currentRound\":3,\"numberOfRounds\":3},"
    "\"listsDetails\":{\"listsLocked\":false,\"options\":{\"allowsText\":true}},\"teamEventOptions\":{}}";

static void test_overview_v2() {
    EventOverview o;
    TEST_ASSERT_TRUE(parseOverview(OVERVIEW_V2, o));
    TEST_ASSERT_EQUAL_STRING("Cobras con la fresca", o.name.c_str());
    TEST_ASSERT_TRUE(o.started);
    TEST_ASSERT_TRUE(o.ended);
    TEST_ASSERT_EQUAL(3, o.numberOfRounds);
    TEST_ASSERT_EQUAL(3, o.currentRound);
}

static void test_overview_missingStatus() {
    EventOverview o;
    TEST_ASSERT_TRUE(parseOverview("{\"name\":\"X\"}", o));
    TEST_ASSERT_EQUAL_STRING("X", o.name.c_str());
    TEST_ASSERT_FALSE(o.started);
    TEST_ASSERT_FALSE(o.ended);
    TEST_ASSERT_EQUAL(0, o.numberOfRounds);
    TEST_ASSERT_EQUAL(0, o.currentRound);
}

static void test_overview_invalidJson_unchanged() {
    EventOverview o;
    o.name = "keep";
    o.currentRound = 4;
    TEST_ASSERT_FALSE(parseOverview("{\"name\":", o));
    TEST_ASSERT_FALSE(parseOverview("", o));
    TEST_ASSERT_EQUAL_STRING("keep", o.name.c_str());
    TEST_ASSERT_EQUAL(4, o.currentRound);
}

// ---- /timer ----

static void test_timer_running() {
    TimerData t;
    TEST_ASSERT_TRUE(parseTimer("{\"timerLength\":9000,\"startTime\":\"2025-11-18T09:00:00Z\","
                                "\"endTime\":\"2025-11-18T11:30:00Z\",\"timerPaused\":false}", t));
    TEST_ASSERT_EQUAL_UINT32(9000, t.timerLength);
    TEST_ASSERT_EQUAL_STRING("2025-11-18T09:00:00Z", t.startTime.c_str());
    TEST_ASSERT_EQUAL_INT64(1763456400, (int64_t) t.startEpoch);
    TEST_ASSERT_EQUAL_INT64(1763456400 + 9000, (int64_t) t.endEpoch);
    TEST_ASSERT_FALSE(t.paused);
    TEST_ASSERT_EQUAL_UINT32(0, t.pausedTimeRemaining);
}

static void test_timer_paused() {
    TimerData t;
    TEST_ASSERT_TRUE(parseTimer("{\"timerLength\":3600,\"startTime\":\"2025-11-18T09:00:00Z\","
                                "\"endTime\":\"2025-11-18T10:00:00Z\",\"timerPaused\":true,"
                                "\"pausedTimeRemaining\":1234}", t));
    TEST_ASSERT_TRUE(t.paused);
    TEST_ASSERT_EQUAL_UINT32(1234, t.pausedTimeRemaining);
}

static void test_timer_pausedFieldMissing() {
    TimerData t;
    t.paused = true;
    TEST_ASSERT_TRUE(parseTimer("{\"timerLength\":3600,\"startTime\":\"2025-11-18T09:00:00Z\","
                                "\"endTime\":\"2025-11-18T10:00:00Z\"}", t));
    TEST_ASSERT_FALSE(t.paused);
}

static void test_timer_emptyObject_noTimer() {
    TimerData t;
    t.timerLength = 3600;
    t.paused = true;
    t.endEpoch = 123;
    TEST_ASSERT_TRUE(parseTimer("{}", t));
    TEST_ASSERT_EQUAL_UINT32(0, t.timerLength);
    TEST_ASSERT_FALSE(t.paused);
    TEST_ASSERT_EQUAL_INT64(0, (int64_t) t.endEpoch);
}

static void test_timer_invalidJson_unchanged() {
    TimerData t;
    t.timerLength = 3600;
    TEST_ASSERT_FALSE(parseTimer("<html>502</html>", t));
    TEST_ASSERT_EQUAL_UINT32(3600, t.timerLength);
}

// ---- ISO time ----

static void test_isoUtc_knownValues() {
    TEST_ASSERT_EQUAL_INT64(0, (int64_t) parseIsoUtc("1970-01-01T00:00:00Z"));
    TEST_ASSERT_EQUAL_INT64(1763456400, (int64_t) parseIsoUtc("2025-11-18T09:00:00Z"));
    TEST_ASSERT_EQUAL_INT64(1767225599, (int64_t) parseIsoUtc("2025-12-31T23:59:59Z"));
    TEST_ASSERT_EQUAL_INT64(1767225600, (int64_t) parseIsoUtc("2026-01-01T00:00:00Z"));
    TEST_ASSERT_EQUAL_INT64(1709164800, (int64_t) parseIsoUtc("2024-02-29T00:00:00Z"));
}

static void test_isoUtc_independentOfTz_andRestoresIt() {
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();
    TEST_ASSERT_EQUAL_INT64(1751365800, (int64_t) parseIsoUtc("2025-07-01T10:30:00Z"));   // summer time
    TEST_ASSERT_EQUAL_STRING("CET-1CEST,M3.5.0,M10.5.0/3", getenv("TZ"));
    unsetenv("TZ");
    tzset();
    TEST_ASSERT_EQUAL_INT64(1751365800, (int64_t) parseIsoUtc("2025-07-01T10:30:00Z"));
    TEST_ASSERT_NULL(getenv("TZ"));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_eventId_plainId);
    RUN_TEST(test_eventId_fromUrl);
    RUN_TEST(test_eventId_empty);
    RUN_TEST(test_eventId_whitespaceKept);
    RUN_TEST(test_overview_running);
    RUN_TEST(test_overview_v2);
    RUN_TEST(test_overview_missingStatus);
    RUN_TEST(test_overview_invalidJson_unchanged);
    RUN_TEST(test_timer_running);
    RUN_TEST(test_timer_paused);
    RUN_TEST(test_timer_pausedFieldMissing);
    RUN_TEST(test_timer_emptyObject_noTimer);
    RUN_TEST(test_timer_invalidJson_unchanged);
    RUN_TEST(test_isoUtc_knownValues);
    RUN_TEST(test_isoUtc_independentOfTz_andRestoresIt);
    return UNITY_END();
}
