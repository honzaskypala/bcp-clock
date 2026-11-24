// Unit tests of the button controlled countdown and the offline mode buttons (lib/core/manualtimer)
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include <unity.h>
#include <Arduino.h>
#include <manualtimer.h>

void setUp() {}
void tearDown() {}

static const long LENGTH = 3600;

static ManualTimer readyTimer() {
    ManualTimer t;
    t.reset(LENGTH);
    return t;
}

// ---- ManualTimer ----

static void test_ready_showsLength() {
    ManualTimer t = readyTimer();
    TEST_ASSERT_EQUAL(MANUAL_READY, t.state);
    TEST_ASSERT_EQUAL(LENGTH, t.remainingAt(1000));
    TEST_ASSERT_EQUAL(LENGTH, t.remainingAt(99999));
    TEST_ASSERT_EQUAL(TIMER_READY, t.timer(LENGTH).phase);
}

static void test_running_countsFromEndTime_alsoAcrossRestart() {
    ManualTimer t = readyTimer();
    TEST_ASSERT_TRUE(t.start(1000));
    TEST_ASSERT_EQUAL(LENGTH, t.remainingAt(1000));
    TEST_ASSERT_EQUAL(LENGTH - 100, t.remainingAt(1100));
    ManualTimer restored = t;   // saved and restored after a power-off
    TEST_ASSERT_EQUAL(LENGTH - 2000, restored.remainingAt(3000));
    TEST_ASSERT_EQUAL(-400, restored.remainingAt(1000 + LENGTH + 400));   // over time
    TEST_ASSERT_FALSE(t.start(1200));                                     // already running
}

static void test_pause_freezes_resumeContinues() {
    ManualTimer t = readyTimer();
    t.start(1000);
    TEST_ASSERT_TRUE(t.pause(1600));
    TEST_ASSERT_EQUAL(LENGTH - 600, t.remainingAt(5000));
    TEST_ASSERT_EQUAL(TIMER_PAUSED, t.timer(LENGTH).phase);
    TEST_ASSERT_EQUAL(TIMER_WAITING, t.timer(LENGTH, true).phase);
    TEST_ASSERT_FALSE(t.pause(1700));                                     // not running
    t.start(5000);
    TEST_ASSERT_EQUAL(LENGTH - 700, t.remainingAt(5100));
}

static void test_timer_runningView() {
    ManualTimer t = readyTimer();
    t.start(1000);
    CountdownTimer c = t.timer(LENGTH);
    TEST_ASSERT_EQUAL(TIMER_RUNNING, c.phase);
    TEST_ASSERT_EQUAL(LENGTH - 10, c.remainingAt(1010));
    TEST_ASSERT_FALSE(c.startPhase);
}

// ---- Offline mode buttons ----

static void test_offline_rightStarts_midPauses_rightResumes() {
    ManualTimer t = readyTimer();
    TEST_ASSERT_EQUAL(TIMER_CHANGED, handleOfflineButton(t, BUTTON_RIGHT, LENGTH, 1000));
    TEST_ASSERT_TRUE(t.isRunning());
    TEST_ASSERT_EQUAL(TIMER_UNCHANGED, handleOfflineButton(t, BUTTON_RIGHT, LENGTH, 1010));
    TEST_ASSERT_EQUAL(TIMER_CHANGED, handleOfflineButton(t, BUTTON_MID_SHORT, LENGTH, 1100));
    TEST_ASSERT_EQUAL(MANUAL_PAUSED, t.state);
    TEST_ASSERT_EQUAL(TIMER_CHANGED, handleOfflineButton(t, BUTTON_RIGHT, LENGTH, 2000));
    TEST_ASSERT_EQUAL(LENGTH - 100, t.remainingAt(2000));
}

static void test_offline_midWhenStopped_nextMode() {
    ManualTimer t = readyTimer();
    TEST_ASSERT_EQUAL(TIMER_NEXT_MODE, handleOfflineButton(t, BUTTON_MID_SHORT, LENGTH, 1000));
    t.start(1000);
    t.pause(1100);
    TEST_ASSERT_EQUAL(TIMER_NEXT_MODE, handleOfflineButton(t, BUTTON_MID_SHORT, LENGTH, 1200));
    TEST_ASSERT_EQUAL(MANUAL_PAUSED, t.state);   // the paused countdown stays
}

static void test_offline_leftResetsOnlyWhenStopped() {
    ManualTimer t = readyTimer();
    TEST_ASSERT_EQUAL(TIMER_UNCHANGED, handleOfflineButton(t, BUTTON_LEFT, LENGTH, 1000));
    t.start(1000);
    TEST_ASSERT_EQUAL(TIMER_UNCHANGED, handleOfflineButton(t, BUTTON_LEFT, LENGTH, 1100));
    TEST_ASSERT_TRUE(t.isRunning());
    t.pause(1100);
    TEST_ASSERT_EQUAL(TIMER_CHANGED, handleOfflineButton(t, BUTTON_LEFT, LENGTH, 1200));
    TEST_ASSERT_EQUAL(MANUAL_READY, t.state);
    TEST_ASSERT_EQUAL(LENGTH, t.remainingAt(1200));
}

static void test_offline_leftAppliesChangedLength() {
    ManualTimer t = readyTimer();
    TEST_ASSERT_EQUAL(TIMER_CHANGED, handleOfflineButton(t, BUTTON_LEFT, 1800, 1000));
    TEST_ASSERT_EQUAL(1800, t.remainingAt(1000));
}

static void test_offline_longMid_ignored() {
    ManualTimer t = readyTimer();
    TEST_ASSERT_EQUAL(TIMER_UNCHANGED, handleOfflineButton(t, BUTTON_MID_LONG, LENGTH, 1000));
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_ready_showsLength);
    RUN_TEST(test_running_countsFromEndTime_alsoAcrossRestart);
    RUN_TEST(test_pause_freezes_resumeContinues);
    RUN_TEST(test_timer_runningView);
    RUN_TEST(test_offline_rightStarts_midPauses_rightResumes);
    RUN_TEST(test_offline_midWhenStopped_nextMode);
    RUN_TEST(test_offline_leftResetsOnlyWhenStopped);
    RUN_TEST(test_offline_leftAppliesChangedLength);
    RUN_TEST(test_offline_longMid_ignored);
    return UNITY_END();
}
