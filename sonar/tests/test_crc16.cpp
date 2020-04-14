#include "gtest/gtest.h"

extern "C" {

#include "src/common/crc16.h"

};

TEST(CRC16, TestEmpty) {
  EXPECT_EQ(crc16(nullptr, 0, 0), 0);
  EXPECT_EQ(crc16(nullptr, 0, CRC16_INITIAL_VALUE), CRC16_INITIAL_VALUE);
}

TEST(CRC16, TestData) {
  // compared against CRC16/CCITT-FALSE output from https://crccalc.com/
  const char* test_data1 = "123";
  EXPECT_EQ(crc16((const uint8_t*)test_data1, strlen(test_data1), CRC16_INITIAL_VALUE), 0x5BCE);
  const char* test_data2 = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  EXPECT_EQ(crc16((const uint8_t*)test_data2, strlen(test_data2), CRC16_INITIAL_VALUE), 0xD8E1);
}
