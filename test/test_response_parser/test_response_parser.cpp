#include <unity.h>
#include <cstring>
#include "net/response_parser.h"

void setUp() {}
void tearDown() {}

void test_parses_status_and_headers() {
    const char* raw =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: audio/wav\r\n"
        "Content-Length: 256\r\n"
        "X-Transcript: hello%20world\r\n"
        "X-Request-Id: abc\r\n"
        "\r\n";
    RespHeaders h;
    TEST_ASSERT_TRUE(resp_parse_headers(raw, strlen(raw), h));
    TEST_ASSERT_EQUAL_INT(200, h.status);
    TEST_ASSERT_EQUAL_STRING("audio/wav", h.content_type);
    TEST_ASSERT_EQUAL_UINT32(256, h.content_length);
    TEST_ASSERT_EQUAL_STRING("hello world", h.x_transcript);
    TEST_ASSERT_EQUAL_STRING("abc", h.x_request_id);
}

void test_rejects_missing_content_length() {
    const char* raw = "HTTP/1.1 200 OK\r\nContent-Type: audio/wav\r\n\r\n";
    RespHeaders h;
    TEST_ASSERT_FALSE(resp_parse_headers(raw, strlen(raw), h));
}

void test_rejects_oversize_content_length() {
    const char* raw =
        "HTTP/1.1 200 OK\r\nContent-Type: audio/wav\r\nContent-Length: 3000000\r\n\r\n";
    RespHeaders h;
    TEST_ASSERT_FALSE(resp_parse_headers(raw, strlen(raw), h));
}

void test_allows_json_body_on_error() {
    const char* raw = "HTTP/1.1 422 Unprocessable\r\n"
                      "Content-Type: application/json\r\n"
                      "Content-Length: 64\r\n\r\n";
    RespHeaders h;
    TEST_ASSERT_TRUE(resp_parse_headers(raw, strlen(raw), h));
    TEST_ASSERT_EQUAL_INT(422, h.status);
    TEST_ASSERT_EQUAL_STRING("application/json", h.content_type);
}

void test_validates_wav_magic() {
    uint8_t good[12] = {'R','I','F','F',0,0,0,0,'W','A','V','E'};
    uint8_t bad[12]  = {'X','X','X','X',0,0,0,0,'W','A','V','E'};
    TEST_ASSERT_TRUE(resp_is_wav(good));
    TEST_ASSERT_FALSE(resp_is_wav(bad));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_parses_status_and_headers);
    RUN_TEST(test_rejects_missing_content_length);
    RUN_TEST(test_rejects_oversize_content_length);
    RUN_TEST(test_allows_json_body_on_error);
    RUN_TEST(test_validates_wav_magic);
    return UNITY_END();
}
