#include <unity.h>
#include "../lib/ipc/include/codec.hpp"
#include "../lib/ipc/include/crc.hpp"


void test_codec_basic() {
using CRC = ipc::CRC16_CCITT;


uint8_t payload[] = {10, 20, 30};
uint8_t encoded[64] = {};
uint8_t decoded[32] = {};


size_t enc_len = ipc::encode_frame<CRC>(payload, sizeof(payload), encoded, sizeof(encoded));
TEST_ASSERT_GREATER_THAN(0, enc_len);


size_t out_len = 0;
ipc::DecodeError err = ipc::DecodeError::None;


bool ok = ipc::decode_frame<CRC>(encoded, enc_len, decoded, sizeof(decoded), &out_len, &err);
TEST_ASSERT_TRUE(ok);
TEST_ASSERT_EQUAL(sizeof(payload), out_len);
TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, decoded, sizeof(payload));
}