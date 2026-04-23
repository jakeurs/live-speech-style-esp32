#include <unity.h>
#include <cstring>
#include "net/styles_api.h"

void setUp() {}
void tearDown() {}

void test_parses_styles_list() {
    const char* body = "["
        "{\"id\":\"jesus\",\"name\":\"Jesus of Nazareth\"},"
        "{\"id\":\"pirate\",\"name\":\"Pirate\"}"
        "]";
    StyleList sl{};
    TEST_ASSERT_TRUE(styles_parse_list(body, strlen(body), sl));
    TEST_ASSERT_EQUAL_INT(2, sl.count);
    TEST_ASSERT_EQUAL_STRING("jesus", sl.ids[0]);
    TEST_ASSERT_EQUAL_STRING("Jesus of Nazareth", sl.names[0]);
    TEST_ASSERT_EQUAL_STRING("pirate", sl.ids[1]);
    TEST_ASSERT_EQUAL_STRING("Pirate", sl.names[1]);
}

void test_parses_health() {
    const char* body = "{\"stt\":\"ok\",\"llm\":\"ok\",\"tts\":\"down\"}";
    HealthStatus h{};
    TEST_ASSERT_TRUE(health_parse(body, strlen(body), h));
    TEST_ASSERT_TRUE(h.stt_ok);
    TEST_ASSERT_TRUE(h.llm_ok);
    TEST_ASSERT_FALSE(h.tts_ok);
}

void test_parses_empty_list_as_failure() {
    const char* body = "[]";
    StyleList sl{};
    TEST_ASSERT_FALSE(styles_parse_list(body, strlen(body), sl));
    TEST_ASSERT_EQUAL_INT(0, sl.count);
}

void test_rejects_malformed_json() {
    const char* body = "not json";
    StyleList sl{};
    TEST_ASSERT_FALSE(styles_parse_list(body, strlen(body), sl));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_parses_styles_list);
    RUN_TEST(test_parses_health);
    RUN_TEST(test_parses_empty_list_as_failure);
    RUN_TEST(test_rejects_malformed_json);
    return UNITY_END();
}
