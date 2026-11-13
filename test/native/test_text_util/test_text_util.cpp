// Unit tests of the string helpers (lib/core/textutil)
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include <unity.h>
#include <Arduino.h>
#include <textutil.h>

void setUp() {}
void tearDown() {}

// ---- Time thresholds ----

static void test_timeToSeconds_formats() {
    TEST_ASSERT_EQUAL(300, timeToSeconds("5"));
    TEST_ASSERT_EQUAL(330, timeToSeconds("5:30"));
    TEST_ASSERT_EQUAL(300, timeToSeconds("05.00"));
    TEST_ASSERT_EQUAL(125, timeToSeconds(" 2:05 "));
    TEST_ASSERT_EQUAL(600, timeToSeconds("10:00"));
}

// Documents current behaviour for unusual input
static void test_timeToSeconds_unusualInput() {
    TEST_ASSERT_EQUAL(0, timeToSeconds(""));
    TEST_ASSERT_EQUAL(0, timeToSeconds("abc"));
    TEST_ASSERT_EQUAL(135, timeToSeconds("1:75"));
    TEST_ASSERT_EQUAL(-60, timeToSeconds("-1"));
    TEST_ASSERT_EQUAL(30, timeToSeconds(":30"));
}

static void test_secondsToTime() {
    TEST_ASSERT_EQUAL_STRING("0:00", secondsToTime(0).c_str());
    TEST_ASSERT_EQUAL_STRING("0:59", secondsToTime(59).c_str());
    TEST_ASSERT_EQUAL_STRING("1:00", secondsToTime(60).c_str());
    TEST_ASSERT_EQUAL_STRING("10:00", secondsToTime(600).c_str());
    TEST_ASSERT_EQUAL_STRING("60:00", secondsToTime(3600).c_str());
}

static void test_timeRoundTrip() {
    for (long n = 0; n <= 7200; n++) {
        TEST_ASSERT_EQUAL(n, timeToSeconds(secondsToTime(n)));
    }
}

// ---- Hostname ----

static void test_hostname_validUnchanged() {
    TEST_ASSERT_EQUAL_STRING("BCP-clock", sanitizeHostname("BCP-clock").c_str());
    TEST_ASSERT_EQUAL_STRING("table-12", sanitizeHostname("table-12").c_str());
}

static void test_hostname_invalidCharsDropped() {
    TEST_ASSERT_EQUAL_STRING("hodinyStl1", sanitizeHostname("hodiny Stůl_1").c_str());
    TEST_ASSERT_EQUAL_STRING("ab", sanitizeHostname("a.b").c_str());
}

static void test_hostname_hyphens() {
    TEST_ASSERT_EQUAL_STRING("a-b", sanitizeHostname("--a-b--").c_str());
    TEST_ASSERT_EQUAL_STRING("a--b", sanitizeHostname("a--b").c_str());
}

static void test_hostname_maxLength() {
    String in = "abcdefghijklmnopqrstuvwxyz0123456789";
    TEST_ASSERT_EQUAL_STRING("abcdefghijklmnopqrstuvwxyz012345", sanitizeHostname(in).c_str());
    // truncated to 32, then the trailing hyphen removed
    TEST_ASSERT_EQUAL_STRING("abcdefghijklmnopqrstuvwxyz01234", sanitizeHostname("abcdefghijklmnopqrstuvwxyz01234-xyz").c_str());
}

static void test_hostname_fallbackToDefault() {
    TEST_ASSERT_EQUAL_STRING("BCP-clock", sanitizeHostname("").c_str());
    TEST_ASSERT_EQUAL_STRING("BCP-clock", sanitizeHostname("---").c_str());
    TEST_ASSERT_EQUAL_STRING("BCP-clock", sanitizeHostname("ěščř").c_str());
}

// ---- Newline separated list ----

static void test_splitList() {
    std::vector<String> v;
    splitList("", v);
    TEST_ASSERT_EQUAL(0, v.size());
    splitList("home", v);
    TEST_ASSERT_EQUAL(1, v.size());
    TEST_ASSERT_EQUAL_STRING("home", v[0].c_str());
    splitList("a\n\nb c\n", v);
    TEST_ASSERT_EQUAL(2, v.size());
    TEST_ASSERT_EQUAL_STRING("a", v[0].c_str());
    TEST_ASSERT_EQUAL_STRING("b c", v[1].c_str());
}

static void test_joinList_roundTrip() {
    std::vector<String> in = { "Home", "Guest net", "Žluťoučký kůň" }, out;
    String joined = joinList(in);
    TEST_ASSERT_EQUAL_STRING("Home\nGuest net\nŽluťoučký kůň", joined.c_str());
    splitList(joined, out);
    TEST_ASSERT_EQUAL(3, out.size());
    for (size_t i = 0; i < in.size(); i++) {
        TEST_ASSERT_EQUAL_STRING(in[i].c_str(), out[i].c_str());
    }
    TEST_ASSERT_EQUAL_STRING("", joinList({}).c_str());
}

// ---- Hashing ----

static void test_fnv1a32_knownVectors() {
    TEST_ASSERT_EQUAL_HEX32(0x811c9dc5, fnv1a32(String("")));
    TEST_ASSERT_EQUAL_HEX32(0xe40c292c, fnv1a32(String("a")));
    TEST_ASSERT_EQUAL_HEX32(0xbf9cf968, fnv1a32(String("foobar")));
}

static void test_makePassKey() {
    TEST_ASSERT_EQUAL_STRING("pwd_e40c292c", makePassKey("a").c_str());
    TEST_ASSERT_EQUAL_STRING("pwd_811c9dc5", makePassKey("").c_str());
    // NVS key limit is 15 characters
    TEST_ASSERT_TRUE(makePassKey("a very long SSID name with spaces and more").length() <= 15);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_timeToSeconds_formats);
    RUN_TEST(test_timeToSeconds_unusualInput);
    RUN_TEST(test_secondsToTime);
    RUN_TEST(test_timeRoundTrip);
    RUN_TEST(test_hostname_validUnchanged);
    RUN_TEST(test_hostname_invalidCharsDropped);
    RUN_TEST(test_hostname_hyphens);
    RUN_TEST(test_hostname_maxLength);
    RUN_TEST(test_hostname_fallbackToDefault);
    RUN_TEST(test_splitList);
    RUN_TEST(test_joinList_roundTrip);
    RUN_TEST(test_fnv1a32_knownVectors);
    RUN_TEST(test_makePassKey);
    return UNITY_END();
}
