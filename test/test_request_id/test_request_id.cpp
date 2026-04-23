#include <unity.h>
#include <string>
#include <set>
#include "util/request_id.h"

void setUp() {}
void tearDown() {}

void test_uuid_v4_format() {
    request_id_seed(0xDEADBEEFu);
    char out[37];
    request_id_new(out);
    // 8-4-4-4-12 with dashes at [8][13][18][23]
    TEST_ASSERT_EQUAL_CHAR('-', out[8]);
    TEST_ASSERT_EQUAL_CHAR('-', out[13]);
    TEST_ASSERT_EQUAL_CHAR('-', out[18]);
    TEST_ASSERT_EQUAL_CHAR('-', out[23]);
    TEST_ASSERT_EQUAL_CHAR('4', out[14]);            // version 4
    TEST_ASSERT_TRUE(out[19] == '8' || out[19] == '9'
                  || out[19] == 'a' || out[19] == 'b'); // variant
    TEST_ASSERT_EQUAL_CHAR('\0', out[36]);
}

void test_uuid_v4_unique() {
    request_id_seed(1u);
    std::set<std::string> seen;
    char out[37];
    for (int i = 0; i < 1000; i++) {
        request_id_new(out);
        seen.insert(out);
    }
    TEST_ASSERT_EQUAL_INT(1000, (int)seen.size());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_uuid_v4_format);
    RUN_TEST(test_uuid_v4_unique);
    return UNITY_END();
}
