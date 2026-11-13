// Unit tests of the RTC snapshot validation (lib/core/snapshot)
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include <unity.h>
#include <Arduino.h>
#include <snapshot.h>
#include <string.h>

void setUp() {}
void tearDown() {}

static const char *ID = "https://www.bestcoastpairings.com/event/AbC123";

static EventSnapshot makeSnapshot() {
    EventSnapshot s = {};
    strncpy(s.name, "Prague Open", sizeof(s.name) - 1);
    s.started = true;
    s.numberOfRounds = 5;
    s.currentRound = 3;
    s.timerLength = 9000;
    s.roundStartEpoch = 1763456400;
    s.roundEndEpoch = 1763465400;
    snapshotSeal(s, ID);
    return s;
}

static void test_sealed_isValid() {
    EventSnapshot s = makeSnapshot();
    TEST_ASSERT_EQUAL_HEX32(SNAPSHOT_MAGIC, s.magic);
    TEST_ASSERT_TRUE(snapshotValid(s, ID));
}

static void test_otherEventId_rejected() {
    EventSnapshot s = makeSnapshot();
    TEST_ASSERT_FALSE(snapshotValid(s, "AbC123"));   // same event, but entered differently
    TEST_ASSERT_FALSE(snapshotValid(s, ""));
}

static void test_anyCorruptedByte_rejected() {
    const EventSnapshot good = makeSnapshot();
    const size_t len = offsetof(EventSnapshot, checksum) + sizeof(good.checksum);
    for (size_t i = 0; i < len; i++) {
        EventSnapshot s = good;
        reinterpret_cast<uint8_t *>(&s)[i] ^= 0x01;
        char msg[32];
        snprintf(msg, sizeof(msg), "byte %u", (unsigned) i);
        TEST_ASSERT_FALSE_MESSAGE(snapshotValid(s, ID), msg);
    }
}

static void test_zeroAndRandomContent_rejected() {
    EventSnapshot s;
    memset(&s, 0, sizeof(s));
    TEST_ASSERT_FALSE(snapshotValid(s, ""));
    memset(&s, 0xA5, sizeof(s));
    TEST_ASSERT_FALSE(snapshotValid(s, ID));
}

static void test_dataChange_needsReseal() {
    EventSnapshot s = makeSnapshot();
    s.currentRound = 4;
    TEST_ASSERT_FALSE(snapshotValid(s, ID));
    snapshotSeal(s, ID);
    TEST_ASSERT_TRUE(snapshotValid(s, ID));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_sealed_isValid);
    RUN_TEST(test_otherEventId_rejected);
    RUN_TEST(test_anyCorruptedByte_rejected);
    RUN_TEST(test_zeroAndRandomContent_rejected);
    RUN_TEST(test_dataChange_needsReseal);
    return UNITY_END();
}
