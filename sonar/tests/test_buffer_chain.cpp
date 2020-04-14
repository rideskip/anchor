#include "gtest/gtest.h"

#include "test_common.h"

extern "C" {

#include "src/common/buffer_chain.h"

};

#define EXPECT_CHAIN_DATA(START_ENTRY_PTR, ...) do { \
    const uint8_t _expected_data[] = {__VA_ARGS__}; \
    EXPECT_TRUE(DataMatches(buffer_chain_to_vector(START_ENTRY_PTR), _expected_data, sizeof(_expected_data))); \
  } while (0)

TEST(BufferChain, TestCreate) {
  const uint8_t data[] = {0x11, 0x12};
  buffer_chain_entry_t entry = {};
  buffer_chain_set_data(&entry, data, sizeof(data));
  EXPECT_EQ(entry.next, nullptr);
  EXPECT_EQ(entry.data, data);
  EXPECT_EQ(entry.length, sizeof(data));
  EXPECT_CHAIN_DATA(&entry, 0x11, 0x12);
}

TEST(BufferChain, TestInsertBack) {
  // create the initial entry
  const uint8_t data1[] = {0x11, 0x12};
  buffer_chain_entry_t entry1 = {};
  buffer_chain_set_data(&entry1, data1, sizeof(data1));
  EXPECT_CHAIN_DATA(&entry1, 0x11, 0x12);

  // insert a second entry at the back
  const uint8_t data2[] = {0x21, 0x22};
  buffer_chain_entry_t entry2 = {};
  buffer_chain_set_data(&entry2, data2, sizeof(data2));
  buffer_chain_push_back(&entry1, &entry2);
  EXPECT_EQ(entry1.next, &entry2);
  EXPECT_EQ(entry2.next, nullptr);
  EXPECT_CHAIN_DATA(&entry1, 0x11, 0x12, 0x21, 0x22);

  // insert a third entry at the back
  const uint8_t data3[] = {0x31, 0x32};
  buffer_chain_entry_t entry3 = {};
  buffer_chain_set_data(&entry3, data3, sizeof(data3));
  buffer_chain_push_back(&entry1, &entry3);
  EXPECT_EQ(entry1.next, &entry2);
  EXPECT_EQ(entry2.next, &entry3);
  EXPECT_EQ(entry3.next, nullptr);
  EXPECT_CHAIN_DATA(&entry1, 0x11, 0x12, 0x21, 0x22, 0x31, 0x32);
}
