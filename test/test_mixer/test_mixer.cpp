#include <unity.h>
#include <cstring>
#include "audio/mixer.h"

void setUp() { mixer_reset(); }
void tearDown() {}

void test_silent_when_no_voices_active() {
    int16_t out[8] = {};
    mixer_render(out, 8, 1.0f);
    for (int i = 0; i < 8; i++) TEST_ASSERT_EQUAL_INT16(0, out[i]);
}

void test_single_voice_copies_samples_with_gain() {
    int16_t src[] = {100, 200, 300, 400};
    mixer_play(0, src, 4, 0.5f, false);
    int16_t out[4];
    mixer_render(out, 4, 1.0f);
    TEST_ASSERT_EQUAL_INT16(50,  out[0]);
    TEST_ASSERT_EQUAL_INT16(100, out[1]);
    TEST_ASSERT_EQUAL_INT16(150, out[2]);
    TEST_ASSERT_EQUAL_INT16(200, out[3]);
}

void test_two_voices_sum() {
    int16_t a[] = {100, 100, 100, 100};
    int16_t b[] = {50, 50, 50, 50};
    mixer_play(0, a, 4, 1.0f, false);
    mixer_play(1, b, 4, 1.0f, false);
    int16_t out[4];
    mixer_render(out, 4, 1.0f);
    for (int i = 0; i < 4; i++) TEST_ASSERT_EQUAL_INT16(150, out[i]);
}

void test_clips_at_int16_range() {
    int16_t a[] = {30000, 30000};
    int16_t b[] = {10000, 10000};
    mixer_play(0, a, 2, 1.0f, false);
    mixer_play(1, b, 2, 1.0f, false);
    int16_t out[2];
    mixer_render(out, 2, 1.0f);
    TEST_ASSERT_EQUAL_INT16(32767, out[0]);
    TEST_ASSERT_EQUAL_INT16(32767, out[1]);
}

void test_loop_wraps() {
    int16_t src[] = {1, 2, 3};
    mixer_play(1, src, 3, 1.0f, true);
    int16_t out[7];
    mixer_render(out, 7, 1.0f);
    int16_t expected[] = {1,2,3,1,2,3,1};
    TEST_ASSERT_EQUAL_INT16_ARRAY(expected, out, 7);
}

void test_non_loop_deactivates_after_end() {
    int16_t src[] = {500, 500};
    mixer_play(0, src, 2, 1.0f, false);
    int16_t out[4];
    mixer_render(out, 4, 1.0f);
    TEST_ASSERT_EQUAL_INT16(500, out[0]);
    TEST_ASSERT_EQUAL_INT16(500, out[1]);
    TEST_ASSERT_EQUAL_INT16(0, out[2]);
    TEST_ASSERT_EQUAL_INT16(0, out[3]);
}

void test_stop_silences_voice() {
    int16_t src[] = {1000, 1000, 1000, 1000};
    mixer_play(1, src, 4, 1.0f, true);
    mixer_stop(1);
    int16_t out[2] = {};
    mixer_render(out, 2, 1.0f);
    TEST_ASSERT_EQUAL_INT16(0, out[0]);
    TEST_ASSERT_EQUAL_INT16(0, out[1]);
}

void test_master_volume_scales_output() {
    int16_t a[] = {1000, 1000};
    mixer_play(0, a, 2, 1.0f, false);
    int16_t out[2];
    mixer_render(out, 2, 0.5f);
    TEST_ASSERT_EQUAL_INT16(500, out[0]);
    TEST_ASSERT_EQUAL_INT16(500, out[1]);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_silent_when_no_voices_active);
    RUN_TEST(test_single_voice_copies_samples_with_gain);
    RUN_TEST(test_two_voices_sum);
    RUN_TEST(test_clips_at_int16_range);
    RUN_TEST(test_loop_wraps);
    RUN_TEST(test_non_loop_deactivates_after_end);
    RUN_TEST(test_stop_silences_voice);
    RUN_TEST(test_master_volume_scales_output);
    return UNITY_END();
}
