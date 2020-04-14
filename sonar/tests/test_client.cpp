#include "gtest/gtest.h"

#include "test_common.h"

extern "C" {

#include "anchor/sonar/client.h"
#include "src/link_layer/timeouts.h"

};

#define PROCESS_RECEIVE_PACKET(...) do { \
    BUILD_PACKET_BUFFER(_buffer, __VA_ARGS__); \
    sonar_client_process(handle_, _buffer, sizeof(_buffer)); \
  } while (0)

#define EXPECT_WRITE_PACKET(...) do { \
    BUILD_PACKET_BUFFER(_buffer, __VA_ARGS__); \
    EXPECT_TRUE(DataMatches(m_write_data, _buffer, sizeof(_buffer))); \
    m_write_data.clear(); \
  } while (0)

SONAR_ATTR_DEF(TEST_ATTR, 0xfff, sizeof(uint32_t), RWN);

static std::vector<uint8_t> m_write_data;
static int m_num_connections;
static int m_num_disconnections;
static int m_attr_num_read_complete;
static int m_attr_num_write_complete;
static int m_attr_num_notify;

static void write_byte(uint8_t byte) {
  m_write_data.push_back(byte);
}

static uint64_t get_system_time_ms(void) {
  return 0;
}

static void connection_changed_callback(bool connected) {
  if (connected) {
    m_num_connections++;
  } else {
    m_num_disconnections++;
  }
}

static void attribute_read_complete_handler(bool success, const void* data, uint32_t length) {
  m_attr_num_read_complete++;
}

static void attribute_write_complete_handler(bool success) {
  m_attr_num_write_complete++;
}

static bool attribute_notify_handler(sonar_attribute_t attr, const void* data, uint32_t length) {
  m_attr_num_notify++;
  return true;
}

class ClientTest : public ::testing::Test {
 protected:
  void SetUp() override {
    m_write_data.clear();
    m_num_connections = 0;
    m_num_disconnections = 0;
    m_attr_num_read_complete = 0;
    m_attr_num_write_complete = 0;
    m_attr_num_notify = 0;

    SONAR_CLIENT_DEF(handle, 1024);
    handle_ = handle;
    const sonar_client_init_t init_client = {
      .write_byte = write_byte,
      .get_system_time_ms = get_system_time_ms,
      .connection_changed_callback = connection_changed_callback,
      .attribute_read_complete_handler = attribute_read_complete_handler,
      .attribute_write_complete_handler = attribute_write_complete_handler,
      .attribute_notify_handler = attribute_notify_handler,
    };
    sonar_client_init(handle, &init_client);
  }

  void TearDown() override {
    EXPECT_TRUE(m_write_data.empty());
    EXPECT_EQ(m_num_connections, 0);
    EXPECT_EQ(m_num_disconnections, 0);
    EXPECT_EQ(m_attr_num_read_complete, 0);
    EXPECT_EQ(m_attr_num_write_complete, 0);
    EXPECT_EQ(m_attr_num_notify, 0);
  }

  sonar_client_handle_t handle_;
};

TEST_F(ClientTest, Full) {
  sonar_client_register(handle_, TEST_ATTR);

  // run the process function
  sonar_client_process(handle_, NULL, 0);
  // should send a connection request
  EXPECT_WRITE_PACKET(0x14, 0x01, 0x00);

  // process the connection response
  PROCESS_RECEIVE_PACKET(0x17, 0x01);
  // should read CTRL_NUM_ATTRS
  EXPECT_WRITE_PACKET(0x10, 0x02, 0x01, 0x11);

  // process the read response
  PROCESS_RECEIVE_PACKET(0x13, 0x02, 0x04, 0x00);
  // should write CTRL_ATTR_OFFSET to 0
  EXPECT_WRITE_PACKET(0x10, 0x03, 0x02, 0x21, 0x00, 0x00);

  // process the write response
  PROCESS_RECEIVE_PACKET(0x13, 0x03);
  // should read CTRL_ATTR_LIST
  EXPECT_WRITE_PACKET(0x10, 0x04, 0x03, 0x11);

  // process the read response
  EXPECT_FALSE(sonar_client_is_connected(handle_));
  PROCESS_RECEIVE_PACKET(0x13, 0x04, 0x01, 0x11, 0x02, 0x31, 0x03, 0x11, 0xff, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
  // should now be connected
  EXPECT_TRUE(sonar_client_is_connected(handle_));
  EXPECT_EQ(m_num_connections, 1);
  m_num_connections = 0;

  // read TEST_ATTR
  EXPECT_TRUE(sonar_client_read(handle_, TEST_ATTR));
  EXPECT_WRITE_PACKET(0x10, 0x05, 0xff, 0x1f);
  // process the response
  PROCESS_RECEIVE_PACKET(0x13, 0x05, 0x44, 0x33, 0x22, 0x11);
  EXPECT_EQ(m_attr_num_read_complete, 1);
  m_attr_num_read_complete = 0;

  // write TEST_ATTR
  const uint32_t test_attr_data = 0xaa11bb22;
  EXPECT_TRUE(sonar_client_write(handle_, TEST_ATTR, (const uint8_t*)&test_attr_data, sizeof(test_attr_data)));
  EXPECT_WRITE_PACKET(0x10, 0x06, 0xff, 0x2f, 0x22, 0xbb, 0x11, 0xaa);
  // process the response
  PROCESS_RECEIVE_PACKET(0x13, 0x06);
  EXPECT_EQ(m_attr_num_write_complete, 1);
  m_attr_num_write_complete = 0;

  // process a notify request for TEST_ATTR
  PROCESS_RECEIVE_PACKET(0x12, 0x00, 0xff, 0x3f, 0x88, 0xaa, 0x99, 0xbb);
  EXPECT_EQ(m_attr_num_notify, 1);
  m_attr_num_notify = 0;
  // expect a response
  EXPECT_WRITE_PACKET(0x11, 0x00);
}
