// Unit tests of the main loop and display decisions (lib/core/policy)
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include <unity.h>
#include <Arduino.h>
#include <policy.h>

void setUp() {}
void tearDown() {}

// ---- decideDisplay ----

static DisplayInputs validEvent(DisplayState state, bool started, bool ended, uint32_t timerLength = 0,
                                const char *name = "Event", int round = 1) {
    DisplayInputs in;
    in.state = state;
    in.redrawRequested = false;
    in.valid = true;
    in.invalid = false;
    in.started = started;
    in.ended = ended;
    in.timerLength = timerLength;
    in.name = name;
    in.currentRound = round;
    return in;
}

static DisplayInputs eventId(DisplayState state, bool valid, bool invalid) {
    DisplayInputs in = validEvent(state, false, false);
    in.valid = valid;
    in.invalid = invalid;
    return in;
}

static void test_notStarted_showsNameOnce() {
    DisplayMemory m;
    TEST_ASSERT_EQUAL(SHOW_EVENT_NAME, decideDisplay(validEvent(DISPLAY_BOOT, false, false), m).action);
    TEST_ASSERT_EQUAL(SHOW_NOTHING_NEW, decideDisplay(validEvent(DISPLAY_EVENT_NAME, false, false), m).action);
}

static void test_ended_showsName() {
    DisplayMemory m;
    TEST_ASSERT_EQUAL(SHOW_EVENT_NAME, decideDisplay(validEvent(DISPLAY_EVENT_COUNTDOWN, true, true, 9000), m).action);
}

static void test_nameChange_redrawsName() {
    DisplayMemory m;
    decideDisplay(validEvent(DISPLAY_BOOT, false, false, 0, "Old"), m);
    TEST_ASSERT_EQUAL(SHOW_EVENT_NAME, decideDisplay(validEvent(DISPLAY_EVENT_NAME, false, false, 0, "New"), m).action);
    TEST_ASSERT_EQUAL(SHOW_NOTHING_NEW, decideDisplay(validEvent(DISPLAY_EVENT_NAME, false, false, 0, "New"), m).action);
}

static void test_runningWithoutTimer_showsRound_andRoundChange() {
    DisplayMemory m;
    TEST_ASSERT_EQUAL(SHOW_EVENT_ROUND, decideDisplay(validEvent(DISPLAY_EVENT_NAME, true, false, 0, "E", 1), m).action);
    TEST_ASSERT_EQUAL(SHOW_NOTHING_NEW, decideDisplay(validEvent(DISPLAY_EVENT_ROUND, true, false, 0, "E", 1), m).action);
    TEST_ASSERT_EQUAL(SHOW_EVENT_ROUND, decideDisplay(validEvent(DISPLAY_EVENT_ROUND, true, false, 0, "E", 2), m).action);
}

static void test_runningWithTimer_showsCountdownOnce() {
    DisplayMemory m;
    TEST_ASSERT_EQUAL(SHOW_COUNTDOWN, decideDisplay(validEvent(DISPLAY_EVENT_ROUND, true, false, 9000), m).action);
    TEST_ASSERT_EQUAL(SHOW_NOTHING_NEW, decideDisplay(validEvent(DISPLAY_EVENT_COUNTDOWN, true, false, 9000), m).action);
}

static void test_timerCleared_backToRound() {
    DisplayMemory m;
    decideDisplay(validEvent(DISPLAY_BOOT, true, false, 0, "E", 2), m);   // remembers round 2
    TEST_ASSERT_EQUAL(SHOW_EVENT_ROUND, decideDisplay(validEvent(DISPLAY_EVENT_COUNTDOWN, true, false, 0, "E", 2), m).action);
}

static void test_unverifiedId_splashUnlessBoot() {
    DisplayMemory m;
    TEST_ASSERT_EQUAL(SHOW_NOTHING_NEW, decideDisplay(eventId(DISPLAY_BOOT, false, false), m).action);
    TEST_ASSERT_EQUAL(SHOW_SPLASH, decideDisplay(eventId(DISPLAY_EVENT_NAME, false, false), m).action);
}

static void test_invalidId_configRequiredOnce_thenValidRestartsServer() {
    DisplayMemory m;
    DisplayDecision d = decideDisplay(eventId(DISPLAY_BOOT, false, true), m);
    TEST_ASSERT_EQUAL(SHOW_CONFIG_REQUIRED, d.action);
    TEST_ASSERT_FALSE(d.restartConfigServer);
    TEST_ASSERT_EQUAL(SHOW_NOTHING_NEW, decideDisplay(eventId(DISPLAY_CONFIG_REQUIRED, false, true), m).action);

    d = decideDisplay(validEvent(DISPLAY_CONFIG_REQUIRED, false, false), m);
    TEST_ASSERT_EQUAL(SHOW_EVENT_NAME, d.action);
    TEST_ASSERT_TRUE(d.restartConfigServer);
    d = decideDisplay(validEvent(DISPLAY_EVENT_NAME, false, false), m);
    TEST_ASSERT_FALSE(d.restartConfigServer);   // only once
}

static void test_configMessage_waitsForRedrawRequest() {
    DisplayMemory m;
    DisplayInputs in = validEvent(DISPLAY_CONFIG_MESSAGE, true, false, 9000);
    TEST_ASSERT_EQUAL(SHOW_NOTHING_NEW, decideDisplay(in, m).action);
    in.redrawRequested = true;
    TEST_ASSERT_EQUAL(SHOW_COUNTDOWN, decideDisplay(in, m).action);
}

// ---- decideRefresh ----

static const int MAX_FAILURES = 5;

static void test_refresh_success_resetsCount() {
    RefreshDecision r = decideRefresh(true, true, 200, 3, MAX_FAILURES);
    TEST_ASSERT_EQUAL(REFRESH_SUCCESS, r.outcome);
    TEST_ASSERT_EQUAL(0, r.failCount);
    r = decideRefresh(true, true, 404, 3, MAX_FAILURES);   // invalid event ID is a successful fetch
    TEST_ASSERT_EQUAL(REFRESH_SUCCESS, r.outcome);
    TEST_ASSERT_EQUAL(0, r.failCount);
}

static void test_refresh_wifiDown_countsTowardReboot() {
    int count = 0;
    for (int i = 1; i <= 4; i++) {
        RefreshDecision r = decideRefresh(false, false, -1, count, MAX_FAILURES);
        TEST_ASSERT_EQUAL(REFRESH_WIFI_DOWN, r.outcome);
        count = r.failCount;
        TEST_ASSERT_EQUAL(i, count);
    }
    RefreshDecision r = decideRefresh(false, false, -1, count, MAX_FAILURES);
    TEST_ASSERT_EQUAL(REFRESH_REBOOT, r.outcome);
    TEST_ASSERT_EQUAL(5, r.failCount);
}

static void test_refresh_wifiDownAndConnectionErrors_shareCount() {
    RefreshDecision r = decideRefresh(false, false, -1, 0, MAX_FAILURES);
    r = decideRefresh(false, false, -1, r.failCount, MAX_FAILURES);
    r = decideRefresh(false, true, -1, r.failCount, MAX_FAILURES);
    r = decideRefresh(false, true, -1, r.failCount, MAX_FAILURES);
    TEST_ASSERT_EQUAL(REFRESH_CONNECTION_ERROR, r.outcome);
    TEST_ASSERT_EQUAL(4, r.failCount);
    r = decideRefresh(false, false, -1, r.failCount, MAX_FAILURES);
    TEST_ASSERT_EQUAL(REFRESH_REBOOT, r.outcome);
}

static void test_refresh_rebootAfterFiveConnectionErrors() {
    int count = 0;
    for (int i = 1; i <= 4; i++) {
        RefreshDecision r = decideRefresh(false, true, -1, count, MAX_FAILURES);
        TEST_ASSERT_EQUAL(REFRESH_CONNECTION_ERROR, r.outcome);
        count = r.failCount;
        TEST_ASSERT_EQUAL(i, count);
    }
    RefreshDecision r = decideRefresh(false, true, -11, count, MAX_FAILURES);
    TEST_ASSERT_EQUAL(REFRESH_REBOOT, r.outcome);
    TEST_ASSERT_EQUAL(5, r.failCount);
}

static void test_refresh_httpError_resetsCount() {
    RefreshDecision r = decideRefresh(false, true, 503, 4, MAX_FAILURES);
    TEST_ASSERT_EQUAL(REFRESH_HTTP_ERROR, r.outcome);
    TEST_ASSERT_EQUAL(0, r.failCount);
    r = decideRefresh(false, true, -1, r.failCount, MAX_FAILURES);
    TEST_ASSERT_EQUAL(REFRESH_CONNECTION_ERROR, r.outcome);   // count starts again
}

static void test_refresh_noRequest() {
    RefreshDecision r = decideRefresh(false, true, 0, 2, MAX_FAILURES);
    TEST_ASSERT_EQUAL(REFRESH_NO_REQUEST, r.outcome);
    TEST_ASSERT_EQUAL(2, r.failCount);
}

// ---- classifyMessage ----
// The display reacts to these prefixes of the WifiMgr messages (wifimgr.cpp)

static void test_classifyMessage() {
    TEST_ASSERT_EQUAL(MESSAGE_ERROR, classifyMessage("Error: Connection failed"));
    TEST_ASSERT_EQUAL(MESSAGE_ERROR, classifyMessage("Error: NTP time sync failed"));
    TEST_ASSERT_EQUAL(MESSAGE_CONNECTING, classifyMessage("Connecting to Home"));
    TEST_ASSERT_EQUAL(MESSAGE_CONNECTED, classifyMessage("Connected to Home"));
    TEST_ASSERT_EQUAL(MESSAGE_OTHER, classifyMessage("Connect to WiFi BCP-clock"));
    TEST_ASSERT_EQUAL(MESSAGE_OTHER, classifyMessage("Connectingx"));
    TEST_ASSERT_EQUAL(MESSAGE_OTHER, classifyMessage("error: lowercase"));
    TEST_ASSERT_EQUAL(MESSAGE_OTHER, classifyMessage(""));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_notStarted_showsNameOnce);
    RUN_TEST(test_ended_showsName);
    RUN_TEST(test_nameChange_redrawsName);
    RUN_TEST(test_runningWithoutTimer_showsRound_andRoundChange);
    RUN_TEST(test_runningWithTimer_showsCountdownOnce);
    RUN_TEST(test_timerCleared_backToRound);
    RUN_TEST(test_unverifiedId_splashUnlessBoot);
    RUN_TEST(test_invalidId_configRequiredOnce_thenValidRestartsServer);
    RUN_TEST(test_configMessage_waitsForRedrawRequest);
    RUN_TEST(test_refresh_success_resetsCount);
    RUN_TEST(test_refresh_wifiDown_countsTowardReboot);
    RUN_TEST(test_refresh_wifiDownAndConnectionErrors_shareCount);
    RUN_TEST(test_refresh_rebootAfterFiveConnectionErrors);
    RUN_TEST(test_refresh_httpError_resetsCount);
    RUN_TEST(test_refresh_noRequest);
    RUN_TEST(test_classifyMessage);
    return UNITY_END();
}
