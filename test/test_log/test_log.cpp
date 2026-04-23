#include <unity.h>
#include <string>
#include "util/log.h"

static std::string captured;

static void capture_sink(const char* line) { captured = line; }

void setUp() { captured.clear(); }
void tearDown() {}

void test_log_format_has_bracketed_timestamp() {
    log_set_sink(capture_sink);
    log_set_clock_ms([]() -> uint32_t { return 12345; });
    log_line(LOG_INFO, "net", "upload_done", "bytes=%d rid=%s", 491520, "abc");
    TEST_ASSERT_TRUE(captured.find("[  12.345]") != std::string::npos);
    TEST_ASSERT_TRUE(captured.find("INFO") != std::string::npos);
    TEST_ASSERT_TRUE(captured.find("net") != std::string::npos);
    TEST_ASSERT_TRUE(captured.find("upload_done") != std::string::npos);
    TEST_ASSERT_TRUE(captured.find("bytes=491520 rid=abc") != std::string::npos);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_log_format_has_bracketed_timestamp);
    return UNITY_END();
}
