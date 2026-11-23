// Unit tests of the alert icon cycle and the connection alert (lib/core/statusalert)
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include <unity.h>
#include <Arduino.h>
#include <statusalert.h>

void setUp() {}
void tearDown() {}

// ---- alertCyclePhase ----

static void test_cycle_noIcon_alwaysContent() {
    TEST_ASSERT_EQUAL(0, alertCyclePhase(0, 0, DISPLAY_EVENT_COUNTDOWN));
    TEST_ASSERT_EQUAL(0, alertCyclePhase(0, 5000, DISPLAY_EVENT_COUNTDOWN));
}

static void test_cycle_oneIcon_alternates() {
    TEST_ASSERT_EQUAL(0, alertCyclePhase(1, 0, DISPLAY_EVENT_COUNTDOWN));
    TEST_ASSERT_EQUAL(0, alertCyclePhase(1, 4999, DISPLAY_EVENT_COUNTDOWN));
    TEST_ASSERT_EQUAL(1, alertCyclePhase(1, 5000, DISPLAY_EVENT_COUNTDOWN));
    TEST_ASSERT_EQUAL(1, alertCyclePhase(1, 9999, DISPLAY_EVENT_ROUND));
    TEST_ASSERT_EQUAL(0, alertCyclePhase(1, 10000, DISPLAY_EVENT_NAME));
    TEST_ASSERT_EQUAL(1, alertCyclePhase(1, 15000, DISPLAY_BOOT));
}

static void test_cycle_twoIcons_eachInTurn() {
    TEST_ASSERT_EQUAL(0, alertCyclePhase(2, 0, DISPLAY_EVENT_COUNTDOWN));
    TEST_ASSERT_EQUAL(1, alertCyclePhase(2, 5000, DISPLAY_EVENT_COUNTDOWN));
    TEST_ASSERT_EQUAL(2, alertCyclePhase(2, 10000, DISPLAY_EVENT_COUNTDOWN));
    TEST_ASSERT_EQUAL(2, alertCyclePhase(2, 14999, DISPLAY_EVENT_COUNTDOWN));
    TEST_ASSERT_EQUAL(0, alertCyclePhase(2, 15000, DISPLAY_EVENT_COUNTDOWN));
}

static void test_cycle_notDuringConfigMessage() {
    TEST_ASSERT_EQUAL(0, alertCyclePhase(1, 5000, DISPLAY_CONFIG_MESSAGE));
    TEST_ASSERT_EQUAL(1, alertCyclePhase(1, 5000, DISPLAY_CONFIG_REQUIRED));
}

// ---- nextConnectionAlert ----

static ConnectionAlert afterOutcomes(const RefreshOutcome *outcomes, int count) {
    ConnectionAlert alert;
    for (int i = 0; i < count; i++) alert = nextConnectionAlert(alert, outcomes[i]);
    return alert;
}

static void test_connection_okByDefault() {
    ConnectionAlert alert;
    TEST_ASSERT_EQUAL(CONNECTION_OK, alert.problem);
    TEST_ASSERT_FALSE(alert.isIconDue());
}

static void test_connection_wifiDown_iconFromFourthFailure() {
    ConnectionAlert alert;
    for (int i = 1; i <= 3; i++) {
        alert = nextConnectionAlert(alert, REFRESH_WIFI_DOWN);
        TEST_ASSERT_EQUAL(CONNECTION_WIFI, alert.problem);
        TEST_ASSERT_EQUAL(i, alert.failures);
        TEST_ASSERT_FALSE(alert.isIconDue());
    }
    alert = nextConnectionAlert(alert, REFRESH_WIFI_DOWN);
    TEST_ASSERT_TRUE(alert.isIconDue());
}

static void test_connection_bcpErrors_bothKindsCount() {
    const RefreshOutcome outcomes[] = { REFRESH_CONNECTION_ERROR, REFRESH_HTTP_ERROR, REFRESH_HTTP_ERROR,
                                        REFRESH_CONNECTION_ERROR };
    ConnectionAlert alert = afterOutcomes(outcomes, 4);
    TEST_ASSERT_EQUAL(CONNECTION_BCP, alert.problem);
    TEST_ASSERT_EQUAL(4, alert.failures);
    TEST_ASSERT_TRUE(alert.isIconDue());
}

static void test_connection_lastFailureDecidesProblem() {
    const RefreshOutcome outcomes[] = { REFRESH_WIFI_DOWN, REFRESH_WIFI_DOWN, REFRESH_WIFI_DOWN,
                                        REFRESH_CONNECTION_ERROR };
    ConnectionAlert alert = afterOutcomes(outcomes, 4);
    TEST_ASSERT_EQUAL(CONNECTION_BCP, alert.problem);
    TEST_ASSERT_TRUE(alert.isIconDue());
}

static void test_connection_successClears() {
    const RefreshOutcome outcomes[] = { REFRESH_WIFI_DOWN, REFRESH_WIFI_DOWN, REFRESH_WIFI_DOWN,
                                        REFRESH_WIFI_DOWN, REFRESH_SUCCESS };
    ConnectionAlert alert = afterOutcomes(outcomes, 5);
    TEST_ASSERT_EQUAL(CONNECTION_OK, alert.problem);
    TEST_ASSERT_EQUAL(0, alert.failures);
    TEST_ASSERT_FALSE(alert.isIconDue());
    alert = nextConnectionAlert(alert, REFRESH_HTTP_ERROR);
    TEST_ASSERT_EQUAL(1, alert.failures);   // count starts again
}

static void test_connection_noRequestClears() {
    ConnectionAlert alert = nextConnectionAlert(ConnectionAlert(), REFRESH_CONNECTION_ERROR);
    alert = nextConnectionAlert(alert, REFRESH_NO_REQUEST);
    TEST_ASSERT_EQUAL(CONNECTION_OK, alert.problem);
}

static void test_connection_rebootKeepsProblem() {
    const RefreshOutcome outcomes[] = { REFRESH_WIFI_DOWN, REFRESH_WIFI_DOWN, REFRESH_WIFI_DOWN,
                                        REFRESH_WIFI_DOWN, REFRESH_REBOOT };
    ConnectionAlert alert = afterOutcomes(outcomes, 5);
    TEST_ASSERT_EQUAL(CONNECTION_WIFI, alert.problem);
    TEST_ASSERT_EQUAL(5, alert.failures);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_cycle_noIcon_alwaysContent);
    RUN_TEST(test_cycle_oneIcon_alternates);
    RUN_TEST(test_cycle_twoIcons_eachInTurn);
    RUN_TEST(test_cycle_notDuringConfigMessage);
    RUN_TEST(test_connection_okByDefault);
    RUN_TEST(test_connection_wifiDown_iconFromFourthFailure);
    RUN_TEST(test_connection_bcpErrors_bothKindsCount);
    RUN_TEST(test_connection_lastFailureDecidesProblem);
    RUN_TEST(test_connection_successClears);
    RUN_TEST(test_connection_noRequestClears);
    RUN_TEST(test_connection_rebootKeepsProblem);
    return UNITY_END();
}
