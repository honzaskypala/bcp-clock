// Unit tests of the round indicator layout (lib/core/countdown)
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include <unity.h>
#include <Arduino.h>
#include <countdown.h>
#include <string>
#include <algorithm>

void setUp() {}
void tearDown() {}

static const int WIDTH = 32;

// '.' off, 'o' inactive, 'A' active
static std::string layout(int current, int total) {
    RoundPixel px[WIDTH + 1];
    px[WIDTH] = (RoundPixel) 0x5A;   // guard: must not be written
    layoutRoundIndicator(current, total, WIDTH, px);
    TEST_ASSERT_EQUAL_HEX8(0x5A, px[WIDTH]);
    std::string s;
    for (int i = 0; i < WIDTH; i++) {
        s += px[i] == ROUND_PIXEL_ACTIVE ? 'A' : px[i] == ROUND_PIXEL_INACTIVE ? 'o' : '.';
    }
    return s;
}

static void test_noRounds_allOff() {
    // regression: division by zero for an event with a timer but zero rounds
    TEST_ASSERT_EQUAL_STRING("................................", layout(1, 0).c_str());
    TEST_ASSERT_EQUAL_STRING("................................", layout(0, -1).c_str());
}

// Layouts of the original implementation (hand-tuned offset table), current round 2
static void test_goldenLayouts() {
    TEST_ASSERT_EQUAL_STRING("..ooooooooo.AAAAAAAAA.ooooooooo.", layout(2, 3).c_str());
    TEST_ASSERT_EQUAL_STRING("..ooooo.AAAAA.ooooo.ooooo.ooooo.", layout(2, 5).c_str());
    TEST_ASSERT_EQUAL_STRING("...ooo.AAA.ooo.ooo.ooo.ooo.ooo..", layout(2, 7).c_str());
    TEST_ASSERT_EQUAL_STRING("....o.A.o.o.o.o.o.o.o.o.o.o.....", layout(2, 12).c_str());
    TEST_ASSERT_EQUAL_STRING(".......o.A.oooooooooooooo.......", layout(2, 16).c_str());
    TEST_ASSERT_EQUAL_STRING(".....o.A.oooooooooooooooooo.....", layout(2, 20).c_str());
    TEST_ASSERT_EQUAL_STRING("oAoooooooooooooooooooooooooooooo", layout(2, 32).c_str());
    TEST_ASSERT_EQUAL_STRING("oAoooooooooooooooooooooooooooooo", layout(2, 40).c_str());
    TEST_ASSERT_EQUAL_STRING(".AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", layout(1, 1).c_str());
    TEST_ASSERT_EQUAL_STRING(".....A.o.o.o.o.o.o.o.o.o.o......", layout(1, 11).c_str());
}

static void test_invariants() {
    char msg[64];
    for (int total = 1; total <= 32; total++) {
        for (int current = 0; current <= total + 1; current++) {
            std::string s = layout(current, total);
            snprintf(msg, sizeof(msg), "total %d current %d: %s", total, current, s.c_str());
            size_t firstActive = s.find('A'), lastActive = s.rfind('A');
            bool valid = current >= 1 && current <= total;
            // active segment exactly when the current round is valid, and only one (contiguous)
            TEST_ASSERT_EQUAL_MESSAGE(valid, firstActive != std::string::npos, msg);
            if (valid) {
                TEST_ASSERT_EQUAL_MESSAGE(std::string::npos, s.substr(firstActive, lastActive - firstActive + 1).find_first_not_of('A'), msg);
            }
            // one segment / pixel per round
            int segments = 0;
            for (int i = 0; i < WIDTH; i++) {
                if (s[i] != '.' && (i == 0 || s[i - 1] == '.')) segments++;
            }
            if (total < WIDTH / 2) {
                TEST_ASSERT_EQUAL_MESSAGE(total, segments, msg);
            } else {
                TEST_ASSERT_EQUAL_MESSAGE(total, (int) (WIDTH - std::count(s.begin(), s.end(), '.')), msg);
            }
            // centred within one pixel
            int left = s.find_first_not_of('.'), right = WIDTH - 1 - s.find_last_not_of('.');
            TEST_ASSERT_INT_WITHIN_MESSAGE(1, left, right, msg);
        }
    }
}

// More than 32 rounds: only the first 32 are shown, rounds 33+ have no active pixel
static void test_moreRoundsThanPixels() {
    TEST_ASSERT_EQUAL_STRING("oooooooooooooooooooooooooooooooA", layout(32, 40).c_str());
    TEST_ASSERT_EQUAL_STRING("oooooooooooooooooooooooooooooooo", layout(33, 40).c_str());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_noRounds_allOff);
    RUN_TEST(test_goldenLayouts);
    RUN_TEST(test_invariants);
    RUN_TEST(test_moreRoundsThanPixels);
    return UNITY_END();
}
