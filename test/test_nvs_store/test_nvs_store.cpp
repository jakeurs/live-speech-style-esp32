#include <unity.h>
#include <cstring>
#include "preferences_fake.h"
#include "config/nvs_store.h"

void setUp() { nvs_test_reset(); }
void tearDown() {}

void test_defaults_returned_when_empty() {
    NvsState s = nvs_load();
    TEST_ASSERT_EQUAL_INT(0, s.style_idx);
    TEST_ASSERT_EQUAL_INT(6, s.volume_x10);
}

void test_save_and_reload_round_trip() {
    NvsState s{};
    s.style_idx = 3;
    strncpy(s.style_id, "pirate", sizeof(s.style_id) - 1);
    s.volume_x10 = 8;
    nvs_save(s);
    NvsState got = nvs_load();
    TEST_ASSERT_EQUAL_INT(3, got.style_idx);
    TEST_ASSERT_EQUAL_STRING("pirate", got.style_id);
    TEST_ASSERT_EQUAL_INT(8, got.volume_x10);
}

void test_rate_limit_suppresses_rapid_writes() {
    nvs_set_clock_ms([]() -> uint32_t { return 1000; });
    NvsState s{};
    strncpy(s.style_id, "a", sizeof(s.style_id) - 1);
    s.volume_x10 = 5;
    TEST_ASSERT_TRUE(nvs_save(s));
    s.volume_x10 = 6;
    TEST_ASSERT_FALSE(nvs_save(s));   // within 1 s window
    nvs_set_clock_ms([]() -> uint32_t { return 2100; });
    TEST_ASSERT_TRUE(nvs_save(s));    // allowed again
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_defaults_returned_when_empty);
    RUN_TEST(test_save_and_reload_round_trip);
    RUN_TEST(test_rate_limit_suppresses_rapid_writes);
    return UNITY_END();
}
