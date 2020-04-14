#include "gtest/gtest.h"

#include "test_common.h"

extern "C" {

#include "src/attribute/server.h"

};

#define READ_EXPECT_RESPONSE(ATTR_ID, ...) do { \
    const uint8_t _response[] = { __VA_ARGS__ }; \
    EXPECT_TRUE(sonar_attribute_server_handle_read_request(handle_, ATTR_ID)); \
    EXPECT_TRUE(DataMatches(m_response_data, _response, sizeof(_response))); \
    m_response_data.clear(); \
  } while (0)

SONAR_ATTR_DEF(TEST_ATTR, 0xff1, sizeof(uint32_t), RW);
SONAR_ATTR_DEF(TEST_ATTR2, 0xff2, sizeof(uint32_t), N);

static uint32_t m_test_attr_num_reads;
static uint32_t m_test_attr_num_writes;
static uint32_t m_test_attr_write_data;
static uint32_t m_test_attr_num_notify_complete;
static bool m_test_attr_notify_complete_success;
static uint32_t m_notify_request_num;
static uint16_t m_notify_request_attribute_id;
static std::vector<uint8_t> m_notify_request_data;
static std::vector<uint8_t> m_response_data;

static bool send_notify_request_function(void* handle, uint16_t attribute_id, const uint8_t* data, uint32_t length) {
  m_notify_request_num++;
  m_notify_request_attribute_id = attribute_id;
  m_notify_request_data.insert(m_notify_request_data.end(), data, data + length);
  return true;
}

static void read_response_handler(void* handle, const uint8_t* data, uint32_t length) {
  m_response_data.insert(m_response_data.end(), data, data + length);
}

static uint32_t read_handler(void* handle, sonar_attribute_t attr, void* response_data, uint32_t response_max_size) {
  m_test_attr_num_reads++;
  if (attr == TEST_ATTR && response_max_size == sizeof(uint32_t)) {
    *(uint32_t*)response_data = 0x11223344;
    return sizeof(uint32_t);
  } else {
    return 0;
  }
}

static bool write_handler(void* handle, sonar_attribute_t attr, const uint8_t* data, uint32_t length) {
  m_test_attr_num_writes++;
  if (attr == TEST_ATTR) {
    if (length != sizeof(m_test_attr_write_data)) {
      return false;
    }
    memcpy(&m_test_attr_write_data, data, length);
    return true;
  } else {
    return false;
  }
}

static void notify_complete_handler(void* handle, bool success) {
  m_test_attr_num_notify_complete++;
  m_test_attr_notify_complete_success = success;
}

class AttributeServerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    m_test_attr_num_reads = 0;
    m_test_attr_num_writes = 0;
    m_test_attr_write_data = 0;
    m_test_attr_num_notify_complete = 0;
    m_test_attr_notify_complete_success = false;
    m_notify_request_num = 0;
    m_notify_request_attribute_id = 0;
    m_notify_request_data.clear();
    static sonar_attribute_server_context_t context;
    handle_ = &context;
    const sonar_attribute_server_init_t init_attribute_server = {
      .send_notify_request_function = send_notify_request_function,
      .read_response_handler = read_response_handler,
      .read_handler = read_handler,
      .write_handler = write_handler,
      .notify_complete_handler = notify_complete_handler,
      .handle = handle_,
    };
    sonar_attribute_server_init(handle_, &init_attribute_server);
    sonar_attribute_server_register(handle_, TEST_ATTR);
    sonar_attribute_server_register(handle_, TEST_ATTR2);
  }

  void TearDown() override {
    EXPECT_EQ(m_test_attr_num_reads, 0);
    EXPECT_EQ(m_test_attr_num_writes, 0);
    EXPECT_EQ(m_test_attr_num_notify_complete, 0);
    EXPECT_EQ(m_notify_request_num, 0);
  }

  sonar_attribute_server_handle_t handle_;
};

TEST_F(AttributeServerTest, HandleValidRead) {
  const uint32_t* data;
  uint32_t data_len = UINT32_MAX;
  READ_EXPECT_RESPONSE(0xff1, 0x44, 0x33, 0x22, 0x11);
  EXPECT_EQ(m_test_attr_num_reads, 1);
  m_test_attr_num_reads = 0;
}

TEST_F(AttributeServerTest, HandleInvalidRead) {
  const uint32_t* data;
  uint32_t data_len;

  // attribute which doesn't exist
  EXPECT_FALSE(sonar_attribute_server_handle_read_request(handle_, 0xfff));
  EXPECT_EQ(m_test_attr_num_reads, 0);

  // attribute which doesn't support reads
  EXPECT_FALSE(sonar_attribute_server_handle_read_request(handle_, 0xff2));
  EXPECT_EQ(m_test_attr_num_reads, 0);
}

TEST_F(AttributeServerTest, HandleValidWrite) {
  const uint32_t data = 0xaabbccdd;
  EXPECT_TRUE(sonar_attribute_server_handle_write_request(handle_, 0xff1, (const uint8_t*)&data, sizeof(data)));
  EXPECT_EQ(m_test_attr_write_data, data);
  EXPECT_EQ(m_test_attr_num_writes, 1);
  m_test_attr_num_writes = 0;
}

TEST_F(AttributeServerTest, HandleInvalidWrite) {
  const uint32_t data = 0xaabbccdd;

  // attribute which doesn't exist
  EXPECT_FALSE(sonar_attribute_server_handle_write_request(handle_, 0xfff, (const uint8_t*)&data, sizeof(data)));

  // attribute which doesn't support writes
  EXPECT_FALSE(sonar_attribute_server_handle_write_request(handle_, 0xff2, (const uint8_t*)&data, sizeof(data)));
}

TEST_F(AttributeServerTest, ValidNotifyRequest) {
  const uint32_t data = 0xabcdabcd;
  EXPECT_TRUE(sonar_attribute_server_notify(handle_, TEST_ATTR2, (const uint8_t*)&data, sizeof(data)));
  EXPECT_EQ(m_notify_request_num, 1);
  m_notify_request_num = 0;
  EXPECT_EQ(m_notify_request_attribute_id, 0xff2);
  EXPECT_EQ(m_notify_request_data.size(), sizeof(data));
  EXPECT_EQ(*(uint32_t*)m_notify_request_data.data(), data);
  m_notify_request_data.clear();
}

TEST_F(AttributeServerTest, InvalidNotifyRequest) {
  // attribute which doesn't support notify requests
  const uint32_t data = 0xabcdabcd;
  EXPECT_FALSE(sonar_attribute_server_notify(handle_, TEST_ATTR, (const uint8_t*)&data, sizeof(data)));
}

TEST_F(AttributeServerTest, NotifyResponse) {
  // success response
  sonar_attribute_server_handle_notify_response(handle_, 0xff2, true);
  EXPECT_EQ(m_test_attr_num_notify_complete, 1);
  m_test_attr_num_notify_complete = 0;
  EXPECT_EQ(m_test_attr_notify_complete_success, true);

  // failed response
  sonar_attribute_server_handle_notify_response(handle_, 0xff2, false);
  EXPECT_EQ(m_test_attr_num_notify_complete, 1);
  m_test_attr_num_notify_complete = 0;
  EXPECT_EQ(m_test_attr_notify_complete_success, false);
}

TEST_F(AttributeServerTest, ControlAttrs) {
  uint32_t data_len;

  // Read CTRL_NUM_ATTRS (should be 2)
  READ_EXPECT_RESPONSE(0x101, 0x02, 0x00);

  // Write to CTRL_ATTR_OFFSET to 0
  const uint16_t initial_attr_offset = 0;
  EXPECT_TRUE(sonar_attribute_server_handle_write_request(handle_, 0x102, (const uint8_t*)&initial_attr_offset, sizeof(initial_attr_offset)));

  // Read CTRL_ATTR_LIST
  READ_EXPECT_RESPONSE(0x103, 0xf2, 0x4f, 0xf1, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);

  // Read back CTRL_ATTR_OFFSET (should still be 0)
  READ_EXPECT_RESPONSE(0x102, 0x00, 0x00);

  // Write CTRL_ATTR_OFFSET to 1
  const uint16_t attr_offset2 = 1;
  EXPECT_TRUE(sonar_attribute_server_handle_write_request(handle_, 0x102, (const uint8_t*)&attr_offset2, sizeof(attr_offset2)));

  // Read CTRL_ATTR_LIST again (with an offset of 1)
  READ_EXPECT_RESPONSE(0x103, 0xf1, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
}
