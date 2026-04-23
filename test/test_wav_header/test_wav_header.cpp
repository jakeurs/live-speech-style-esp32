#include <unity.h>
#include <cstring>
#include "config/wav_header.h"

void setUp() {}
void tearDown() {}

void test_header_canonical_layout_16k_mono_s16() {
    uint8_t hdr[44];
    wav_header_build(hdr, /*pcm_bytes*/ 320000);
    TEST_ASSERT_EQUAL_MEMORY("RIFF", hdr + 0, 4);
    TEST_ASSERT_EQUAL_MEMORY("WAVE", hdr + 8, 4);
    TEST_ASSERT_EQUAL_MEMORY("fmt ", hdr + 12, 4);
    TEST_ASSERT_EQUAL_MEMORY("data", hdr + 36, 4);
    TEST_ASSERT_EQUAL_UINT32(16,    *(uint32_t*)(hdr + 16));
    TEST_ASSERT_EQUAL_UINT16(1,     *(uint16_t*)(hdr + 20));
    TEST_ASSERT_EQUAL_UINT16(1,     *(uint16_t*)(hdr + 22));
    TEST_ASSERT_EQUAL_UINT32(16000, *(uint32_t*)(hdr + 24));
    TEST_ASSERT_EQUAL_UINT32(32000, *(uint32_t*)(hdr + 28));
    TEST_ASSERT_EQUAL_UINT16(2,     *(uint16_t*)(hdr + 32));
    TEST_ASSERT_EQUAL_UINT16(16,    *(uint16_t*)(hdr + 34));
    TEST_ASSERT_EQUAL_UINT32(320000, *(uint32_t*)(hdr + 40));
    TEST_ASSERT_EQUAL_UINT32(320036, *(uint32_t*)(hdr + 4));
}

void test_patch_length() {
    uint8_t hdr[44];
    wav_header_build(hdr, 0);
    wav_header_patch_length(hdr, 480000);
    TEST_ASSERT_EQUAL_UINT32(480036, *(uint32_t*)(hdr + 4));
    TEST_ASSERT_EQUAL_UINT32(480000, *(uint32_t*)(hdr + 40));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_header_canonical_layout_16k_mono_s16);
    RUN_TEST(test_patch_length);
    return UNITY_END();
}
