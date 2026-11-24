// Unit tests of the display brightness (lib/core/brightness)
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include <unity.h>
#include <brightness.h>

void setUp() {}
void tearDown() {}

// ---- normalizeBrightness ----

static void test_normalize_keepsSteps() {
    TEST_ASSERT_EQUAL_UINT8(10, normalizeBrightness(10));
    TEST_ASSERT_EQUAL_UINT8(50, normalizeBrightness(50));
    TEST_ASSERT_EQUAL_UINT8(100, normalizeBrightness(100));
}

static void test_normalize_roundsToStep() {
    TEST_ASSERT_EQUAL_UINT8(40, normalizeBrightness(44));
    TEST_ASSERT_EQUAL_UINT8(50, normalizeBrightness(45));
    TEST_ASSERT_EQUAL_UINT8(90, normalizeBrightness(94));
    TEST_ASSERT_EQUAL_UINT8(100, normalizeBrightness(95));
}

static void test_normalize_clamps() {
    TEST_ASSERT_EQUAL_UINT8(10, normalizeBrightness(0));     // not a number: String::toInt() = 0
    TEST_ASSERT_EQUAL_UINT8(10, normalizeBrightness(-50));
    TEST_ASSERT_EQUAL_UINT8(10, normalizeBrightness(14));
    TEST_ASSERT_EQUAL_UINT8(100, normalizeBrightness(101));
    TEST_ASSERT_EQUAL_UINT8(100, normalizeBrightness(255));
    TEST_ASSERT_EQUAL_UINT8(100, normalizeBrightness(100000));
}

// ---- brightnessLevel ----

static void test_level_mapsPercent() {
    TEST_ASSERT_EQUAL_UINT8(255, brightnessLevel(100));
    TEST_ASSERT_EQUAL_UINT8(127, brightnessLevel(50));
    TEST_ASSERT_EQUAL_UINT8(25, brightnessLevel(10));
}

static void test_level_clampsAbove100() {
    TEST_ASSERT_EQUAL_UINT8(255, brightnessLevel(200));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_normalize_keepsSteps);
    RUN_TEST(test_normalize_roundsToStep);
    RUN_TEST(test_normalize_clamps);
    RUN_TEST(test_level_mapsPercent);
    RUN_TEST(test_level_clampsAbove100);
    return UNITY_END();
}
