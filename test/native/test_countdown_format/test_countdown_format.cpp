// Unit tests of the countdown text and colour (lib/core/countdown)
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include <unity.h>
#include <Arduino.h>
#include <countdown.h>
#include <limits.h>

void setUp() {}
void tearDown() {}

static const long YELLOW = 600, RED = 0;   // default thresholds

static CountdownView run(long remaining, uint32_t timerLength) {
    return formatCountdown(remaining, timerLength, false, 0, YELLOW, RED, false);
}

static void assertView(const char *text, CountdownColor color, bool hmmss, int x, const CountdownView& v) {
    TEST_ASSERT_EQUAL_STRING(text, v.text);
    TEST_ASSERT_EQUAL(color, v.color);
    TEST_ASSERT_EQUAL(hmmss, v.hmmss);
    TEST_ASSERT_EQUAL(x, v.x);
}

// ---- Running round ----

static void test_running_shortRound_mmss() {
    assertView("29:59", COUNTDOWN_WHITE, false, 5, run(1799, 3600));
    assertView("10:01", COUNTDOWN_WHITE, false, 5, run(601, 3600));
}

static void test_running_longRound_hmmss() {
    assertView("2:30:00", COUNTDOWN_WHITE, true, 3, run(9000, 9000));
    assertView("0:10:01", COUNTDOWN_WHITE, true, 3, run(601, 9000));
}

static void test_running_tenHourRound_twoDigitHours() {
    assertView("10:00:00", COUNTDOWN_WHITE, true, 1, run(36000, 36000));
    assertView("09:59:59", COUNTDOWN_WHITE, true, 1, run(35999, 36000));
    assertView("00:00:01", COUNTDOWN_YELLOW, true, 1, run(1, 36000));
    assertView("00:00", COUNTDOWN_RED, false, 5, run(0, 3600));   // short rounds unchanged
}

// ---- Thresholds ----

static void test_yellowThreshold_inclusive() {
    TEST_ASSERT_EQUAL(COUNTDOWN_WHITE, run(601, 3600).color);
    TEST_ASSERT_EQUAL(COUNTDOWN_YELLOW, run(600, 3600).color);
    TEST_ASSERT_EQUAL(COUNTDOWN_YELLOW, run(1, 3600).color);
}

static void test_redThreshold_inclusive_defaultZero() {
    assertView("00:00", COUNTDOWN_RED, false, 5, run(0, 3600));
    CountdownView v = formatCountdown(60, 3600, false, 0, YELLOW, 60, false);
    TEST_ASSERT_EQUAL(COUNTDOWN_RED, v.color);
    v = formatCountdown(61, 3600, false, 0, YELLOW, 60, false);
    TEST_ASSERT_EQUAL(COUNTDOWN_YELLOW, v.color);
}

static void test_thresholdsBeforeStart_takePrecedence() {
    // yellow above the timer length: the time to start is never shown green
    CountdownView v = formatCountdown(700, 600, false, 0, 1000, RED, false);
    assertView("11:40", COUNTDOWN_YELLOW, false, 5, v);
}

// ---- Before round start ----

static void test_beforeStart_shortRound_green() {
    assertView("00:01", COUNTDOWN_GREEN, false, 5, run(3601, 3600));
    assertView("00:00", COUNTDOWN_GREEN, false, 5, run(3600, 3600));   // == length: not started yet
    assertView("00:01", COUNTDOWN_GREEN, false, 5, run(601, 600));
    assertView("59:59", COUNTDOWN_GREEN, false, 5, run(3600 + 3599, 3600));
    assertView("1:00:00", COUNTDOWN_GREEN, true, 3, run(3600 + 3600, 3600));
}

static void test_beforeStart_longRound_greenHmmss() {
    // == length of a long round counts as running
    assertView("2:30:00", COUNTDOWN_WHITE, true, 3, run(9000, 9000));
    // time to start of a long round stays in h:mm:ss
    assertView("0:00:01", COUNTDOWN_GREEN, true, 3, run(9001, 9000));
}

static void test_beforeStart_twoDigitHours() {
    assertView("00:00:01", COUNTDOWN_GREEN, true, 1, run(36001, 36000));
    // time to start >= 10 h of a shorter round
    assertView("10:00:00", COUNTDOWN_GREEN, true, 1, run(3600 + 36000, 3600));
    assertView("9:59:59", COUNTDOWN_GREEN, true, 3, run(3600 + 35999, 3600));
}

// ---- Over time ----

static void test_overTime_mmss() {
    assertView("-00:01", COUNTDOWN_RED, false, 1, run(-1, 3600));
    assertView("-59:59", COUNTDOWN_RED, false, 1, run(-3599, 3600));
    assertView("-//://", COUNTDOWN_RED, false, 1, run(-3600, 3600));
}

static void test_overTime_hmmss() {
    assertView("-0:00:01", COUNTDOWN_RED, true, -1, run(-1, 9000));
    assertView("-9:59:59", COUNTDOWN_RED, true, -1, run(-35999, 9000));
    assertView("-/://://", COUNTDOWN_RED, true, -1, run(-36000, 9000));
}

static void test_overTime_tenHourRound_singleDigitHours() {
    // "-hh:mm:ss" would not fit the display
    assertView("-0:00:01", COUNTDOWN_RED, true, -1, run(-1, 36000));
}

// ---- Paused ----

static void test_paused_showsPausedRemaining_blinks() {
    assertView("10:00", COUNTDOWN_MAGENTA, false, 5, formatCountdown(-500, 3600, true, 600, YELLOW, RED, true));
    assertView("10:00", COUNTDOWN_BLACK, false, 5, formatCountdown(-500, 3600, true, 600, YELLOW, RED, false));
}

static void test_paused_precedesThresholds() {
    // paused below red threshold: still blinking magenta, not red
    CountdownView v = formatCountdown(0, 3600, true, 0, YELLOW, 60, true);
    assertView("00:00", COUNTDOWN_MAGENTA, false, 5, v);
}

static void test_paused_longRound_hmmss() {
    assertView("1:15:00", COUNTDOWN_MAGENTA, true, 3, formatCountdown(0, 9000, true, 4500, YELLOW, RED, true));
    assertView("01:15:00", COUNTDOWN_MAGENTA, true, 1, formatCountdown(0, 40000, true, 4500, YELLOW, RED, true));
}

// ---- Invariants over a range of values ----

static void test_invariants_fontCharsAndHours() {
    const uint32_t lengths[] = { 0, 600, 3599, 3600, 3601, 5400, 9000, 35999, 36000, 43200 };
    for (uint32_t len : lengths) {
        for (long rem = -40000; rem <= 75000; rem += 7) {
            CountdownView v = run(rem, len);
            const char *t = v.text;
            // only characters of the time fonts F4x6 / F3x5_Fixed ('-' to ':')
            for (const char *p = t; *p; p++) {
                TEST_ASSERT_TRUE_MESSAGE(*p >= '-' && *p <= ':', t);
            }
            if (!v.hmmss) {
                // mm:ss never hides hours: |shown value| < 1 h, or overflow
                long shown = v.color == COUNTDOWN_GREEN ? rem - (long) len : rem;
                bool overflow = strcmp(t, "-//://") == 0;
                TEST_ASSERT_TRUE_MESSAGE(overflow ? shown <= -3600 : shown > -3600 && shown < 3600, t);
                TEST_ASSERT_TRUE(strlen(t) <= 6);
            } else {
                // "h:mm:ss", "hh:mm:ss" or "-h:mm:ss"; every glyph advances 4 px, the last one is 3 px
                // wide ('-' starts 1 px into its cell), so the text fits the 32 px display
                TEST_ASSERT_TRUE_MESSAGE(strlen(t) <= 8, t);
                TEST_ASSERT_TRUE_MESSAGE(v.x + 4 * (int) strlen(t) - 1 <= 32, t);
                TEST_ASSERT_TRUE_MESSAGE(t[0] == '-' ? v.x >= -1 : v.x >= 0, t);
            }
        }
    }
}

// Documents a known limitation: a paused timer of a round <= 1 h with more than 1 h remaining
// loses the hours (mm:ss font). BCP never reports more paused time than the timer length.
// mm:ss shows total minutes, up to 99:59
static void test_pausedOverOneHourInShortRound_totalMinutes() {
    CountdownView v = formatCountdown(0, 600, true, 4000, YELLOW, RED, true);
    TEST_ASSERT_FALSE(v.hmmss);
    TEST_ASSERT_EQUAL_STRING("66:40", v.text);
}

// ---- Acoustic signals ----

static void test_crossing_yellowAtThreshold() {
    TEST_ASSERT_EQUAL(BEEP_NONE, detectThresholdCrossing(602, 601, 600, 0));
    TEST_ASSERT_EQUAL(BEEP_YELLOW, detectThresholdCrossing(601, 600, 600, 0));
    TEST_ASSERT_EQUAL(BEEP_NONE, detectThresholdCrossing(600, 599, 600, 0));
}

static void test_crossing_redAtThreshold() {
    TEST_ASSERT_EQUAL(BEEP_NONE, detectThresholdCrossing(2, 1, 600, 0));
    TEST_ASSERT_EQUAL(BEEP_RED, detectThresholdCrossing(1, 0, 600, 0));
    TEST_ASSERT_EQUAL(BEEP_NONE, detectThresholdCrossing(0, -1, 600, 0));
}

static void test_crossing_skippedSecond() {
    // a redraw may skip a second
    TEST_ASSERT_EQUAL(BEEP_YELLOW, detectThresholdCrossing(601, 599, 600, 0));
}

static void test_crossing_bothAtOnce_redWins() {
    TEST_ASSERT_EQUAL(BEEP_RED, detectThresholdCrossing(700, -5, 600, 0));
    TEST_ASSERT_EQUAL(BEEP_RED, detectThresholdCrossing(301, 300, 300, 300));
}

static void test_crossing_negativeRedThreshold() {
    TEST_ASSERT_EQUAL(BEEP_NONE, detectThresholdCrossing(0, -1, 600, -60));
    TEST_ASSERT_EQUAL(BEEP_RED, detectThresholdCrossing(-59, -60, 600, -60));
}

static void test_crossing_noPreviousValue() {
    TEST_ASSERT_EQUAL(BEEP_NONE, detectThresholdCrossing(LONG_MIN, 600, 600, 0));
    TEST_ASSERT_EQUAL(BEEP_NONE, detectThresholdCrossing(LONG_MIN, 0, 600, 0));
}

static void test_crossing_constantWhilePaused() {
    TEST_ASSERT_EQUAL(BEEP_NONE, detectThresholdCrossing(600, 600, 600, 0));
}

static void test_crossing_upwards_noBeep() {
    // new round or extended timer
    TEST_ASSERT_EQUAL(BEEP_NONE, detectThresholdCrossing(-30, 3600, 600, 0));
}

static void test_beepPattern_yellow() {
    TEST_ASSERT_EQUAL_UINT32(2000, beepPatternLength(BEEP_YELLOW));
    TEST_ASSERT_TRUE(isBeepOn(BEEP_YELLOW, 0));
    TEST_ASSERT_TRUE(isBeepOn(BEEP_YELLOW, 1999));
    TEST_ASSERT_FALSE(isBeepOn(BEEP_YELLOW, 2000));
    TEST_ASSERT_FALSE(isBeepOn(BEEP_YELLOW, 3000));
}

static void test_beepPattern_red() {
    TEST_ASSERT_EQUAL_UINT32(8000, beepPatternLength(BEEP_RED));
    const unsigned long on[] = { 0, 1999, 3000, 4999, 6000, 7999 };
    const unsigned long off[] = { 2000, 2999, 5000, 5999, 8000, 9000 };
    for (unsigned long ms : on) TEST_ASSERT_TRUE(isBeepOn(BEEP_RED, ms));
    for (unsigned long ms : off) TEST_ASSERT_FALSE(isBeepOn(BEEP_RED, ms));
}

static void test_beepPattern_none() {
    TEST_ASSERT_EQUAL_UINT32(0, beepPatternLength(BEEP_NONE));
    TEST_ASSERT_FALSE(isBeepOn(BEEP_NONE, 0));
}

// ---- CountdownTimer (offline countdown, chess clock, BCP) ----

static CountdownTimer timer(TimerPhase phase, long remaining, uint32_t length) {
    CountdownTimer t;
    t.phase = phase;
    t.length = length;
    t.endEpoch = 1000 + remaining;   // now = 1000
    t.remaining = remaining;
    return t;
}

static void test_timer_ready_whiteWithoutThresholds() {
    assertView("60:00", COUNTDOWN_WHITE, false, 5, formatCountdown(timer(TIMER_READY, 3600, 3600), 1000, YELLOW, RED, false));
    // a ready countdown below the yellow threshold is still white
    assertView("05:00", COUNTDOWN_WHITE, false, 5, formatCountdown(timer(TIMER_READY, 300, 300), 1000, YELLOW, RED, false));
}

static void test_timer_runningAtFullLength_noTimeToStart() {
    // without startPhase the full length is the running countdown, not "00:00" in green
    assertView("60:00", COUNTDOWN_WHITE, false, 5, formatCountdown(timer(TIMER_RUNNING, 3600, 3600), 1000, YELLOW, RED, false));
    assertView("1:30:00", COUNTDOWN_WHITE, true, 3, formatCountdown(timer(TIMER_RUNNING, 5400, 5400), 1000, YELLOW, RED, false));
}

static void test_timer_running_thresholdsAndOverTime() {
    assertView("10:00", COUNTDOWN_YELLOW, false, 5, formatCountdown(timer(TIMER_RUNNING, 600, 3600), 1000, YELLOW, RED, false));
    assertView("-00:30", COUNTDOWN_RED, false, 1, formatCountdown(timer(TIMER_RUNNING, -30, 3600), 1000, YELLOW, RED, false));
}

static void test_timer_paused_blinks_alsoOverTime() {
    assertView("20:00", COUNTDOWN_MAGENTA, false, 5, formatCountdown(timer(TIMER_PAUSED, 1200, 3600), 1000, YELLOW, RED, true));
    assertView("20:00", COUNTDOWN_BLACK, false, 5, formatCountdown(timer(TIMER_PAUSED, 1200, 3600), 1000, YELLOW, RED, false));
    // paused in over time keeps the sign (the BCP pause value was unsigned)
    assertView("-00:30", COUNTDOWN_MAGENTA, false, 1, formatCountdown(timer(TIMER_PAUSED, -30, 3600), 1000, YELLOW, RED, true));
}

static void test_timer_startPhase_bcpTimeToStart() {
    CountdownTimer t = timer(TIMER_RUNNING, 3600 + 120, 3600);
    t.startPhase = true;
    assertView("02:00", COUNTDOWN_GREEN, false, 5, formatCountdown(t, 1000, YELLOW, RED, false));
}

static void test_timer_remainingAt() {
    CountdownTimer t = timer(TIMER_RUNNING, 100, 3600);
    TEST_ASSERT_EQUAL(100, t.remainingAt(1000));
    TEST_ASSERT_EQUAL(40, t.remainingAt(1060));
    t.phase = TIMER_PAUSED;
    TEST_ASSERT_EQUAL(100, t.remainingAt(5000));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_running_shortRound_mmss);
    RUN_TEST(test_running_longRound_hmmss);
    RUN_TEST(test_running_tenHourRound_twoDigitHours);
    RUN_TEST(test_yellowThreshold_inclusive);
    RUN_TEST(test_redThreshold_inclusive_defaultZero);
    RUN_TEST(test_thresholdsBeforeStart_takePrecedence);
    RUN_TEST(test_beforeStart_shortRound_green);
    RUN_TEST(test_beforeStart_longRound_greenHmmss);
    RUN_TEST(test_beforeStart_twoDigitHours);
    RUN_TEST(test_overTime_mmss);
    RUN_TEST(test_overTime_hmmss);
    RUN_TEST(test_overTime_tenHourRound_singleDigitHours);
    RUN_TEST(test_paused_showsPausedRemaining_blinks);
    RUN_TEST(test_paused_precedesThresholds);
    RUN_TEST(test_paused_longRound_hmmss);
    RUN_TEST(test_invariants_fontCharsAndHours);
    RUN_TEST(test_pausedOverOneHourInShortRound_totalMinutes);
    RUN_TEST(test_crossing_yellowAtThreshold);
    RUN_TEST(test_crossing_redAtThreshold);
    RUN_TEST(test_crossing_skippedSecond);
    RUN_TEST(test_crossing_bothAtOnce_redWins);
    RUN_TEST(test_crossing_negativeRedThreshold);
    RUN_TEST(test_crossing_noPreviousValue);
    RUN_TEST(test_crossing_constantWhilePaused);
    RUN_TEST(test_crossing_upwards_noBeep);
    RUN_TEST(test_beepPattern_yellow);
    RUN_TEST(test_beepPattern_red);
    RUN_TEST(test_beepPattern_none);
    RUN_TEST(test_timer_ready_whiteWithoutThresholds);
    RUN_TEST(test_timer_runningAtFullLength_noTimeToStart);
    RUN_TEST(test_timer_running_thresholdsAndOverTime);
    RUN_TEST(test_timer_paused_blinks_alsoOverTime);
    RUN_TEST(test_timer_startPhase_bcpTimeToStart);
    RUN_TEST(test_timer_remainingAt);
    return UNITY_END();
}
