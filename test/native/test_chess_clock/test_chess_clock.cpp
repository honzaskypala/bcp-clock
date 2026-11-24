// Unit tests of the chess clock (lib/core/chessclock)
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include <unity.h>
#include <Arduino.h>
#include <chessclock.h>

void setUp() {}
void tearDown() {}

static const long PLAYER = 1800;   // round 1:00:00, half per player

static ChessClock resetClock() {
    ChessClock c;
    c.reset(PLAYER);
    return c;
}

static void test_reset_bothFull_noActive() {
    ChessClock c = resetClock();
    TEST_ASSERT_TRUE(c.isReset());
    CountdownDisplay d = chessCountdown(c, PLAYER, 600, 0);
    TEST_ASSERT_EQUAL(2, d.count);
    TEST_ASSERT_EQUAL(-1, d.active);
    TEST_ASSERT_EQUAL(TIMER_READY, d.timers[0].phase);
    TEST_ASSERT_EQUAL(PLAYER, d.timers[1].remainingAt(5000));
    TEST_ASSERT_EQUAL(0, d.numberOfRounds);
}

static void test_thresholdsHalved() {
    CountdownDisplay d = chessCountdown(resetClock(), PLAYER, 600, -60);
    TEST_ASSERT_EQUAL(300, d.yellowThreshold);
    TEST_ASSERT_EQUAL(-30, d.redThreshold);
}

static void test_rightStartsLeftPlayer_againNothing() {
    ChessClock c = resetClock();
    TEST_ASSERT_EQUAL(TIMER_CHANGED, handleChessButton(c, BUTTON_RIGHT, PLAYER, 1000));
    TEST_ASSERT_EQUAL(0, c.active);
    TEST_ASSERT_TRUE(c.players[0].isRunning());
    TEST_ASSERT_EQUAL(TIMER_UNCHANGED, handleChessButton(c, BUTTON_RIGHT, PLAYER, 1010));
    TEST_ASSERT_EQUAL(PLAYER - 20, c.players[0].remainingAt(1020));
}

static void test_leftStartsRightPlayer() {
    ChessClock c = resetClock();
    TEST_ASSERT_EQUAL(TIMER_CHANGED, handleChessButton(c, BUTTON_LEFT, PLAYER, 1000));
    TEST_ASSERT_EQUAL(1, c.active);
}

static void test_switch_stopsOther_keepsItsTime() {
    ChessClock c = resetClock();
    handleChessButton(c, BUTTON_RIGHT, PLAYER, 1000);
    TEST_ASSERT_EQUAL(TIMER_CHANGED, handleChessButton(c, BUTTON_LEFT, PLAYER, 1100));
    TEST_ASSERT_EQUAL(1, c.active);
    TEST_ASSERT_EQUAL(PLAYER - 100, c.players[0].remainingAt(9999));
    TEST_ASSERT_EQUAL(PLAYER - 50, c.players[1].remainingAt(1150));
    handleChessButton(c, BUTTON_RIGHT, PLAYER, 1200);
    TEST_ASSERT_EQUAL(PLAYER - 100, c.players[1].remainingAt(9999));
    TEST_ASSERT_EQUAL(PLAYER - 110, c.players[0].remainingAt(1210));
    CountdownDisplay d = chessCountdown(c, PLAYER, 600, 0);
    TEST_ASSERT_EQUAL(0, d.active);
    TEST_ASSERT_EQUAL(TIMER_RUNNING, d.timers[0].phase);
    TEST_ASSERT_EQUAL(TIMER_WAITING, d.timers[1].phase);   // not blinking
}

static void test_midPauses_thenResumeOrSwitch() {
    ChessClock c = resetClock();
    handleChessButton(c, BUTTON_RIGHT, PLAYER, 1000);
    TEST_ASSERT_EQUAL(TIMER_CHANGED, handleChessButton(c, BUTTON_MID_SHORT, PLAYER, 1100));
    TEST_ASSERT_EQUAL(0, c.active);
    TEST_ASSERT_FALSE(c.players[0].isRunning());
    CountdownDisplay d = chessCountdown(c, PLAYER, 600, 0);
    TEST_ASSERT_EQUAL(TIMER_PAUSED, d.timers[0].phase);    // blinks magenta, bar stays
    // resume with the other player's button switches the turn
    TEST_ASSERT_EQUAL(TIMER_CHANGED, handleChessButton(c, BUTTON_LEFT, PLAYER, 2000));
    TEST_ASSERT_EQUAL(1, c.active);
    TEST_ASSERT_EQUAL(PLAYER - 100, c.players[0].remainingAt(9999));
    TEST_ASSERT_EQUAL(PLAYER - 10, c.players[1].remainingAt(2010));
}

static void test_pausedResumeSamePlayer() {
    ChessClock c = resetClock();
    handleChessButton(c, BUTTON_RIGHT, PLAYER, 1000);
    handleChessButton(c, BUTTON_MID_SHORT, PLAYER, 1100);
    TEST_ASSERT_EQUAL(TIMER_CHANGED, handleChessButton(c, BUTTON_RIGHT, PLAYER, 5000));
    TEST_ASSERT_EQUAL(PLAYER - 110, c.players[0].remainingAt(5010));
}

static void test_midWhenPaused_resets_thenNextMode() {
    ChessClock c = resetClock();
    handleChessButton(c, BUTTON_RIGHT, PLAYER, 1000);
    handleChessButton(c, BUTTON_MID_SHORT, PLAYER, 1100);
    TEST_ASSERT_EQUAL(TIMER_CHANGED, handleChessButton(c, BUTTON_MID_SHORT, PLAYER, 1200));
    TEST_ASSERT_TRUE(c.isReset());
    TEST_ASSERT_EQUAL(PLAYER, c.players[0].remainingAt(1200));
    TEST_ASSERT_EQUAL(TIMER_NEXT_MODE, handleChessButton(c, BUTTON_MID_SHORT, PLAYER, 1300));
}

static void test_runsOnAfterRestore_overTime() {
    ChessClock c = resetClock();
    handleChessButton(c, BUTTON_LEFT, PLAYER, 1000);
    ChessClock restored = c;   // saved before a power-off
    TEST_ASSERT_EQUAL(-100, restored.players[1].remainingAt(1000 + PLAYER + 100));
    TEST_ASSERT_EQUAL(1, restored.active);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_reset_bothFull_noActive);
    RUN_TEST(test_thresholdsHalved);
    RUN_TEST(test_rightStartsLeftPlayer_againNothing);
    RUN_TEST(test_leftStartsRightPlayer);
    RUN_TEST(test_switch_stopsOther_keepsItsTime);
    RUN_TEST(test_midPauses_thenResumeOrSwitch);
    RUN_TEST(test_pausedResumeSamePlayer);
    RUN_TEST(test_midWhenPaused_resets_thenNextMode);
    RUN_TEST(test_runsOnAfterRestore_overTime);
    return UNITY_END();
}
