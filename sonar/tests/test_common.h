#pragma once

#include "gtest/gtest.h"

extern "C" {

#include "src/common/buffer_chain.h"
#include "src/common/crc16.h"

};

#define BUILD_PACKET_BUFFER(NAME, ...) \
  const uint8_t NAME##_data[] = {__VA_ARGS__}; \
  uint8_t NAME[sizeof(NAME##_data) + 4]; \
  const uint16_t _crc = crc16(NAME##_data, sizeof(NAME##_data), CRC16_INITIAL_VALUE); \
  NAME[0] = 0x7e; \
  memcpy(&NAME[1], NAME##_data, sizeof(NAME##_data)); \
  NAME[sizeof(NAME) - 3] = _crc & 0xff; \
  NAME[sizeof(NAME) - 2] = _crc >> 8; \
  NAME[sizeof(NAME) - 1] = 0x7e;

static ::testing::AssertionResult DataMatches(const std::vector<uint8_t>& actual, const uint8_t* expected, size_t expected_length) {
  if (actual.size() != expected_length) {
    return ::testing::AssertionFailure()
        << "actual length (" << actual.size()
        << ") != expected length (" << expected_length << ")";
  }
  for (size_t i = 0; i < expected_length; i++){
    if (expected[i] != actual.at(i)) {
      std::stringstream stream;
      stream << "actual[" << i
        << "] (0x" << std::hex << std::setw(2) << std::setfill('0') << int(actual.at(i)) << ") != expected[" << i
        << "] (0x" << std::hex << std::setw(2) << std::setfill('0') << int(expected[i]) << ")";
      return ::testing::AssertionFailure() << stream.str();
    }
  }

  return ::testing::AssertionSuccess();
}

static std::vector<uint8_t> buffer_chain_to_vector(const buffer_chain_entry_t* data) {
  std::vector<uint8_t> result;
  FOREACH_BUFFER_CHAIN_ENTRY(data, entry) {
    result.insert(result.end(), entry->data, entry->data + entry->length);
  }
  return result;
}
