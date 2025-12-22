#include <unity.h>
#include "../lib/ipc/include/cobs.hpp"


void test_cobs_basic() {
uint8_t input[] = {1, 2, 0, 3, 4};
uint8_t encoded[32] = {};
uint8_t decoded[32] = {};


size_t enc_len = ipc::cobs_encode(input, sizeof(input), encoded, sizeof(encoded));
TEST_ASSERT_GREATER_THAN(0, enc_len);


size_t dec_len = ipc::cobs_decode(encoded, enc_len, decoded, sizeof(decoded));
TEST_ASSERT_EQUAL(sizeof(input), dec_len);
TEST_ASSERT_EQUAL_UINT8_ARRAY(input, decoded, sizeof(input));
}