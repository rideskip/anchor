#include "gtest/gtest.h"

#include "test_common.h"

extern "C" {

#include "src/attribute/client.h"

};

SONAR_ATTR_DEF(TEST_ATTR, 0xff1, sizeof(uint32_t), RW);
SONAR_ATTR_DEF(TEST_ATTR2, 0xff2, sizeof(uint32_t), N);

static uint32_t m_test_attr_num_read_complete;
static bool m_test_attr_read_complete_success;
static uint32_t m_test_attr_read_complete_data;
static uint32_t m_test_attr_num_write_complete;
static bool m_test_attr_write_complete_success;
static uint32_t m_test_attr_num_notifies;
static uint32_t m_test_notify_data;
static uint32_t m_read_request_num;
static uint32_t m_read_request_attribute_id;
static uint32_t m_write_request_num;
static uint16_t m_write_request_attribute_id;
static std::vector<uint8_t> m_write_request_data;
static int m_num_connections;
static int m_num_disconnections;

static bool send_read_request_function(void* handle, uint16_t attribute_id) {
  m_read_request_num++;
  m_read_request_attribute_id = attribute_id;
  return true;
}

static bool send_write_request_function(void* handle, uint16_t attribute_id, const uint8_t* data, uint32_t length) {
  m_write_request_num++;
  m_write_request_attribute_id = attribute_id;
  m_write_request_data.insert(m_write_request_data.end(), data, data + length);
  return true;
}

static void read_complete_handler(void* handle, bool success, const uint8_t* data, uint32_t length) {
  m_test_attr_num_read_complete++;
  m_test_attr_read_complete_success = success;
  if (success) {
    m_test_attr_read_complete_data = *(const uint32_t*)data;
  }
}

static void write_complete_handler(void* handle, bool success) {
  m_test_attr_num_write_complete++;
  m_test_attr_write_complete_success = success;
}

static bool notify_handler(void* handle, sonar_attribute_t attr, const uint8_t* data, uint32_t length) {
  m_test_attr_num_notifies++;
  if (length == sizeof(uint32_t)) {
    m_test_notify_data = *(uint32_t*)data;
  }
  return true;
}

static void connection_changed_callback(void* handle, bool connected) {
  if (connected) {
    m_num_connections++;
  } else {
    m_num_disconnections++;
  }
}

class AttributeClientTest : public ::testing::Test {
 protected:
  void SetUp() override {
    m_test_attr_num_read_complete = 0;
    m_test_attr_read_complete_success = 0;
    m_test_attr_read_complete_data = 0;
    m_test_attr_num_write_complete = 0;
    m_test_attr_write_complete_success = false;
    m_test_attr_num_notifies = 0;
    m_test_notify_data = 0;
    m_read_request_num = 0;
    m_read_request_attribute_id = 0;
    m_write_request_num = 0;
    m_write_request_attribute_id = 0;
    m_write_request_data.clear();
    m_num_connections = 0;
    m_num_disconnections = 0;

    static sonar_attribute_client_context_t context;
    const sonar_attribute_client_init_t init_attribute_client = {
      .send_read_request_function = send_read_request_function,
      .send_write_request_function = send_write_request_function,
      .connection_changed_callback = connection_changed_callback,
      .read_complete_handler = read_complete_handler,
      .write_complete_handler = write_complete_handler,
      .notify_handler = notify_handler,
      .handle = NULL,
    };
    handle_ = &context;
    sonar_attribute_client_init(handle_, &init_attribute_client);
    sonar_attribute_client_register(handle_, TEST_ATTR);
    sonar_attribute_client_register(handle_, TEST_ATTR2);

    // Run (and test) the connection process as it's required before any of the other tests can run

    // Set the state to connected and expect a CTRL_NUM_ATTRS read request
    sonar_attribute_client_low_level_connection_changed(handle_, true);
    EXPECT_EQ(m_read_request_num, 1);
    m_read_request_num = 0;
    EXPECT_EQ(m_read_request_attribute_id, 0x101);

    // Respond to the CTRL_NUM_ATTRS read request and expect a CTRL_ATTR_OFFSET write request
    const uint16_t num = 5;
    sonar_attribute_client_handle_read_response(handle_, 0x101, true, (const uint8_t*)&num, sizeof(num));
    EXPECT_EQ(m_write_request_num, 1);
    m_write_request_num = 0;
    EXPECT_EQ(m_write_request_attribute_id, 0x102);
    EXPECT_EQ(m_write_request_data.size(), sizeof(uint16_t));
    EXPECT_EQ(*(uint16_t*)m_write_request_data.data(), 0);
    m_write_request_data.clear();

    // Respond to the CTRL_ATTR_OFFSET write request and expect a CTRL_ATTR_LIST read request
    sonar_attribute_client_handle_write_response(handle_, 0x102, true);
    EXPECT_EQ(m_read_request_num, 1);
    m_read_request_num = 0;
    EXPECT_EQ(m_read_request_attribute_id, 0x103);

    // Respond to the CTRL_ATTR_LIST read request
    const uint16_t attr_list[8] = { 0x4ff2, 0x3ff1, 0x1103, 0x3102, 0x1101 };
    sonar_attribute_client_handle_read_response(handle_, 0x103, true, (const uint8_t*)&attr_list, sizeof(attr_list));
    EXPECT_EQ(m_num_connections, 1);
    m_num_connections = 0;
  }

  void TearDown() override {
    sonar_attribute_client_low_level_connection_changed(handle_, false);
    EXPECT_EQ(m_num_disconnections, 1);

    EXPECT_EQ(m_test_attr_num_read_complete, 0);
    EXPECT_EQ(m_test_attr_num_write_complete, 0);
    EXPECT_EQ(m_test_attr_num_notifies, 0);
    EXPECT_EQ(m_read_request_num, 0);
    EXPECT_EQ(m_write_request_num, 0);
    EXPECT_TRUE(m_write_request_data.empty());
    EXPECT_EQ(m_num_connections, 0);
  }

  sonar_attribute_client_handle_t handle_;
};

TEST_F(AttributeClientTest, ValidReadRequest) {
  EXPECT_TRUE(sonar_attribute_client_read(handle_, TEST_ATTR));
  EXPECT_EQ(m_read_request_num, 1);
  m_read_request_num = 0;
  EXPECT_EQ(m_read_request_attribute_id, 0xff1);
}

TEST_F(AttributeClientTest, InvalidReadRequest) {
  // attribute which doesn't support read requests
  EXPECT_FALSE(sonar_attribute_client_read(handle_, TEST_ATTR2));
}

TEST_F(AttributeClientTest, ReadResponse) {
  const uint32_t data = 0x44556677;

  // success response
  sonar_attribute_client_handle_read_response(handle_, 0xff1, true, (const uint8_t*)&data, sizeof(data));
  EXPECT_EQ(m_test_attr_num_read_complete, 1);
  m_test_attr_num_read_complete = 0;
  EXPECT_EQ(m_test_attr_read_complete_success, true);
  EXPECT_EQ(m_test_attr_read_complete_data, data);

  // failed response
  sonar_attribute_client_handle_read_response(handle_, 0xff1, false, NULL, 0);
  EXPECT_EQ(m_test_attr_num_read_complete, 1);
  m_test_attr_num_read_complete = 0;
  EXPECT_EQ(m_test_attr_read_complete_success, false);
}

TEST_F(AttributeClientTest, ValidWriteRequest) {
  const uint32_t value = 0xabcdabcd;
  EXPECT_TRUE(sonar_attribute_client_write(handle_, TEST_ATTR, (const uint8_t*)&value, sizeof(value)));
  EXPECT_EQ(m_write_request_num, 1);
  m_write_request_num = 0;
  EXPECT_EQ(m_write_request_attribute_id, 0xff1);
  EXPECT_EQ(m_write_request_data.size(), sizeof(uint32_t));
  EXPECT_EQ(*(uint32_t*)m_write_request_data.data(), 0xabcdabcd);
  m_write_request_data.clear();
}

TEST_F(AttributeClientTest, InvalidWriteRequest) {
  // attribute which doesn't support write requests
  const uint32_t value = 0;
  EXPECT_FALSE(sonar_attribute_client_write(handle_, TEST_ATTR2, (const uint8_t*)&value, sizeof(value)));
}

TEST_F(AttributeClientTest, WriteResponse) {
  // success response
  sonar_attribute_client_handle_write_response(handle_, 0xff1, true);
  EXPECT_EQ(m_test_attr_num_write_complete, 1);
  m_test_attr_num_write_complete = 0;
  EXPECT_EQ(m_test_attr_write_complete_success, true);

  // failed response
  sonar_attribute_client_handle_write_response(handle_, 0xff1, false);
  EXPECT_EQ(m_test_attr_num_write_complete, 1);
  m_test_attr_num_write_complete = 0;
  EXPECT_EQ(m_test_attr_write_complete_success, false);
}

TEST_F(AttributeClientTest, HandleValidNotify) {
  const uint32_t data = 0x12345678;
  EXPECT_TRUE(sonar_attribute_client_handle_notify_request(handle_, 0xff2, (const uint8_t*)&data, sizeof(data)));
  EXPECT_EQ(m_test_attr_num_notifies, 1);
  m_test_attr_num_notifies = 0;
  EXPECT_EQ(m_test_notify_data, data);
}

TEST_F(AttributeClientTest, HandleInvalidNotify) {
  const uint32_t data = 0xaabbccdd;

  // attribute which doesn't exist
  EXPECT_FALSE(sonar_attribute_client_handle_notify_request(handle_, 0xfff, (const uint8_t*)&data, sizeof(data)));

  // attribute which doesn't support notify
  EXPECT_FALSE(sonar_attribute_client_handle_notify_request(handle_, 0xff1, (const uint8_t*)&data, sizeof(data)));
}

TEST_F(AttributeClientTest, TestDisconnect) {
  sonar_attribute_client_low_level_connection_changed(handle_, false);
  EXPECT_EQ(m_num_disconnections, 1);
  m_num_disconnections = 0;

  // responses should fail / be ignored
  const uint32_t data = 0x44556677;
  sonar_attribute_client_handle_read_response(handle_, 0xff1, true, (const uint8_t*)&data, sizeof(data));
  sonar_attribute_client_handle_write_response(handle_, 0xff1, true);

  // requests should fail
  EXPECT_FALSE(sonar_attribute_client_handle_notify_request(handle_, 0xff2, (const uint8_t*)&data, sizeof(data)));
}
