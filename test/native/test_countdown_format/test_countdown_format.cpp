// Unit tests of the countdown text and colour (lib/core/countdown)
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include <unity.h>
#include <Arduino.h>
#include <countdown.h>

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
}

// ---- Invariants over a range of values ----

static void test_invariants_fontCharsAndHours() {
    const uint32_t lengths[] = { 0, 600, 3599, 3600, 3601, 5400, 9000, 35999 };
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
                // single digit hours only (rounds >= 10 h are a TODO): "h:mm:ss" or "-h:mm:ss"
                TEST_ASSERT_TRUE_MESSAGE(strlen(t) <= 8, t);
            }
        }
    }
}

// Documents a known limitation: a paused timer of a round <= 1 h with more than 1 h remaining
// loses the hours (mm:ss font). BCP never reports more paused time than the timer length.
static void test_knownLimitation_pausedOverOneHourInShortRound() {
    CountdownView v = formatCountdown(0, 600, true, 4000, YELLOW, RED, true);
    TEST_ASSERT_FALSE(v.hmmss);
    TEST_ASSERT_EQUAL_STRING("06:40", v.text);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_running_shortRound_mmss);
    RUN_TEST(test_running_longRound_hmmss);
    RUN_TEST(test_yellowThreshold_inclusive);
    RUN_TEST(test_redThreshold_inclusive_defaultZero);
    RUN_TEST(test_thresholdsBeforeStart_takePrecedence);
    RUN_TEST(test_beforeStart_shortRound_green);
    RUN_TEST(test_beforeStart_longRound_greenHmmss);
    RUN_TEST(test_overTime_mmss);
    RUN_TEST(test_overTime_hmmss);
    RUN_TEST(test_paused_showsPausedRemaining_blinks);
    RUN_TEST(test_paused_precedesThresholds);
    RUN_TEST(test_paused_longRound_hmmss);
    RUN_TEST(test_invariants_fontCharsAndHours);
    RUN_TEST(test_knownLimitation_pausedOverOneHourInShortRound);
    return UNITY_END();
}
