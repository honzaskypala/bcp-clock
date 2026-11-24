// Unit tests of the button press detection (lib/core/buttons) and the mode order (lib/core/modeid.h)
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include <unity.h>
#include <Arduino.h>
#include <buttons.h>
#include <modeid.h>

void setUp() {}
void tearDown() {}

// Samples the level every 10 ms from fromMs (inclusive) to toMs (exclusive), returns the presses seen
struct Seen { int down = 0, shortPress = 0, longPress = 0; unsigned long longAt = 0; };

static Seen sample(PressDetector& d, bool pressed, unsigned long fromMs, unsigned long toMs) {
    Seen s;
    for (unsigned long t = fromMs; t < toMs; t += 10) {
        switch (d.update(pressed, t)) {
            case PRESS_DOWN: s.down++; break;
            case PRESS_SHORT: s.shortPress++; break;
            case PRESS_LONG: s.longPress++; s.longAt = t; break;
            default: break;
        }
    }
    return s;
}

// ---- PressDetector ----

static void test_idle_nothing() {
    PressDetector d;
    Seen s = sample(d, false, 0, 5000);
    TEST_ASSERT_EQUAL(0, s.down + s.shortPress + s.longPress);
}

static void test_shortPress_downThenShortAtRelease() {
    PressDetector d;
    sample(d, false, 0, 100);
    Seen s = sample(d, true, 100, 400);
    TEST_ASSERT_EQUAL(1, s.down);
    TEST_ASSERT_EQUAL(0, s.shortPress);
    s = sample(d, false, 400, 600);
    TEST_ASSERT_EQUAL(1, s.shortPress);
    TEST_ASSERT_EQUAL(0, s.longPress);
}

static void test_longPress_reportedWhileHeld_once_noShortAtRelease() {
    PressDetector d;
    sample(d, false, 0, 100);
    Seen s = sample(d, true, 100, 3000);
    TEST_ASSERT_EQUAL(1, s.down);
    TEST_ASSERT_EQUAL(1, s.longPress);
    TEST_ASSERT_UINT32_WITHIN(50, 100 + BUTTON_DEBOUNCE_MS + MID_LONG_PRESS_MS, s.longAt);
    s = sample(d, false, 3000, 3200);
    TEST_ASSERT_EQUAL(0, s.shortPress);
}

static void test_bounce_ignored() {
    PressDetector d;
    sample(d, false, 0, 100);
    Seen s;
    // contact bounce: 10 ms pulses
    for (unsigned long t = 100; t < 160; t += 10) {
        if (d.update((t / 10) % 2 == 0, t) != PRESS_NONE) s.down++;
    }
    s = sample(d, true, 160, 300);
    TEST_ASSERT_EQUAL(1, s.down);
    s = sample(d, false, 300, 500);
    TEST_ASSERT_EQUAL(1, s.shortPress);
}

static void test_shortGlitch_noPress() {
    PressDetector d;
    sample(d, false, 0, 100);
    Seen s = sample(d, true, 100, 120);   // 20 ms, below debounce
    s = sample(d, false, 120, 400);
    TEST_ASSERT_EQUAL(0, s.down + s.shortPress + s.longPress);
}

static void test_heldAtStart_ignoredUntilReleased() {
    PressDetector d(true);
    Seen s = sample(d, true, 0, 3000);
    TEST_ASSERT_EQUAL(0, s.down + s.shortPress + s.longPress);
    s = sample(d, false, 3000, 3200);
    TEST_ASSERT_EQUAL(0, s.shortPress);
    s = sample(d, true, 3200, 3400);       // next press counts
    TEST_ASSERT_EQUAL(1, s.down);
    s = sample(d, false, 3400, 3600);
    TEST_ASSERT_EQUAL(1, s.shortPress);
}

static void test_twoShortPresses() {
    PressDetector d;
    sample(d, false, 0, 100);
    sample(d, true, 100, 200);
    Seen s = sample(d, false, 200, 300);
    TEST_ASSERT_EQUAL(1, s.shortPress);
    sample(d, true, 300, 400);
    s = sample(d, false, 400, 500);
    TEST_ASSERT_EQUAL(1, s.shortPress);
}

// ---- Mode order ----

static void test_findModeIndex() {
    const ModeId all[] = { MODE_BCP, MODE_OFFLINE, MODE_CHESS };
    TEST_ASSERT_EQUAL(0, findModeIndex(all, 3, MODE_BCP));
    TEST_ASSERT_EQUAL(2, findModeIndex(all, 3, MODE_CHESS));
    const ModeId noBcp[] = { MODE_OFFLINE, MODE_CHESS };
    TEST_ASSERT_EQUAL(0, findModeIndex(noBcp, 2, MODE_BCP));   // not in the build: first mode
    TEST_ASSERT_EQUAL(1, findModeIndex(noBcp, 2, MODE_CHESS));
    TEST_ASSERT_EQUAL(0, findModeIndex(all, 3, 200));          // unknown stored value
}

static void test_nextModeIndex_wraps() {
    TEST_ASSERT_EQUAL(1, nextModeIndex(0, 3));
    TEST_ASSERT_EQUAL(0, nextModeIndex(2, 3));
    TEST_ASSERT_EQUAL(0, nextModeIndex(0, 1));
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_idle_nothing);
    RUN_TEST(test_shortPress_downThenShortAtRelease);
    RUN_TEST(test_longPress_reportedWhileHeld_once_noShortAtRelease);
    RUN_TEST(test_bounce_ignored);
    RUN_TEST(test_shortGlitch_noPress);
    RUN_TEST(test_heldAtStart_ignoredUntilReleased);
    RUN_TEST(test_twoShortPresses);
    RUN_TEST(test_findModeIndex);
    RUN_TEST(test_nextModeIndex_wraps);
    return UNITY_END();
}
