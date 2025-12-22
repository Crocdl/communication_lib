#include <unity.h>
#include "../lib/ipc/include/crc.hpp"


void test_crc_basic() {
const uint8_t data[] = {1, 2, 3, 4, 5};


ipc::CRC16_CCITT crc;
crc.update(data, sizeof(data));
auto value1 = crc.finalize();


crc.reset();
crc.update(data, sizeof(data));
auto value2 = crc.finalize();


TEST_ASSERT_EQUAL(value1, value2);
}