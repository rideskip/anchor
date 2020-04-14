#include "gtest/gtest.h"

#include "test_common.h"

extern "C" {

#include "anchor/sonar/server.h"
#include "src/link_layer/timeouts.h"

};

#define PROCESS_RECEIVE_PACKET(...) do { \
    BUILD_PACKET_BUFFER(_buffer, __VA_ARGS__); \
    sonar_server_process(handle_, _buffer, sizeof(_buffer)); \
  } while (0)

#define EXPECT_WRITE_PACKET(...) do { \
    BUILD_PACKET_BUFFER(_buffer, __VA_ARGS__); \
    EXPECT_TRUE(DataMatches(m_write_data, _buffer, sizeof(_buffer))); \
    m_write_data.clear(); \
  } while (0)

SONAR_SERVER_ATTR_DEF(TestAttr, TEST_ATTR, 0xfff, sizeof(uint32_t), RWN);

static sonar_server_handle_t m_handle;
static std::vector<uint8_t> m_write_data;
static uint64_t m_system_time;
static int m_num_connections;
static int m_num_disconnections;
static int m_attr_num_read;
static int m_attr_num_write;
static uint32_t m_attr_write_data;
static int m_attr_num_notify_complete;
static bool m_attr_notify_complete_success;

static void write_byte(uint8_t byte) {
  m_write_data.push_back(byte);
}

static uint64_t get_system_time_ms(void) {
  return m_system_time;
}

static void connection_changed_callback(sonar_server_handle_t handle, bool connected) {
  if (connected) {
    m_num_connections++;
  } else {
    m_num_disconnections++;
  }
}

static bool TestAttr_write_handler(const void* data, uint32_t length) {
  m_attr_num_write++;
  if (length != sizeof(m_attr_write_data)) {
    return false;
  }
  memcpy(&m_attr_write_data, data, length);
  return true;
}

static uint32_t TestAttr_read_handler(void* response_data, uint32_t response_max_size) {
  m_attr_num_read++;
  if (response_max_size == sizeof(uint32_t)) {
    *(uint32_t*)response_data = 0x11223344;
    return sizeof(uint32_t);
  } else {
    return 0;
  }
}

static void attribute_notify_complete_handler(sonar_server_handle_t handle, bool success) {
  m_attr_num_notify_complete++;
  m_attr_notify_complete_success = success;
}

class ServerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    m_write_data.clear();
    m_system_time = 0;
    m_num_connections = 0;
    m_num_disconnections = 0;
    m_attr_num_read = 0;
    m_attr_num_write = 0;
    m_attr_num_notify_complete = 0;

    SONAR_SERVER_DEF(handle, 1024);
    handle_ = handle;
    m_handle = handle_;
    const sonar_server_init_t init_server = {
      .write_byte = write_byte,
      .get_system_time_ms = get_system_time_ms,
      .connection_changed_callback = connection_changed_callback,
      .attribute_notify_complete_handler = attribute_notify_complete_handler,
    };
    sonar_server_init(handle, &init_server);
  }

  void TearDown() override {
    EXPECT_TRUE(m_write_data.empty());
    EXPECT_EQ(m_num_connections, 0);
    EXPECT_EQ(m_num_disconnections, 0);
    EXPECT_EQ(m_attr_num_read, 0);
    EXPECT_EQ(m_attr_num_write, 0);
    EXPECT_EQ(m_attr_num_notify_complete, 0);
  }
  sonar_server_handle_t handle_;
};

TEST_F(ServerTest, Idle) {
  // run process a bunch of times (longer than any of our timeouts) with no data coming in
  for (int i = 0; i < CONNECTION_TIMEOUT_MS; i++) {
    sonar_server_process(handle_, NULL, 0);
    m_system_time += 5;
  }
}

TEST_F(ServerTest, Disconnection) {
  // receive a connection request and make sure we're connected and send a response
  PROCESS_RECEIVE_PACKET(0x14, 0x00, 0x80);
  EXPECT_WRITE_PACKET(0x17, 0x00);
  EXPECT_TRUE(sonar_server_is_connected(handle_));
  EXPECT_EQ(m_num_connections, 1);
  m_num_connections = 0;

  // increment the system time by more than our connection timeout and make sure we disconnect
  m_system_time += CONNECTION_TIMEOUT_MS;
  sonar_server_process(handle_, NULL, 0);
  EXPECT_EQ(m_num_disconnections, 1);
  m_num_disconnections = 0;

  // receive a new connection request and make sure we're connected and send a response
  PROCESS_RECEIVE_PACKET(0x14, 0x01, 0x80);
  EXPECT_WRITE_PACKET(0x17, 0x01);
  EXPECT_TRUE(sonar_server_is_connected(handle_));
  EXPECT_EQ(m_num_connections, 1);
  m_num_connections = 0;
}

TEST_F(ServerTest, Read) {
  // register our attribute
  sonar_server_register(handle_, TEST_ATTR);

  // connect (also tested by ServerTest.Connection)
  PROCESS_RECEIVE_PACKET(0x14, 0x00, 0x80);
  EXPECT_WRITE_PACKET(0x17, 0x00);
  EXPECT_TRUE(sonar_server_is_connected(handle_));
  EXPECT_EQ(m_num_connections, 1);
  m_num_connections = 0;

  // process a read request and make sure we send a response
  PROCESS_RECEIVE_PACKET(0x10, 0x01, 0xff, 0x1f);
  EXPECT_WRITE_PACKET(0x13, 0x01, 0x44, 0x33, 0x22, 0x11);
  EXPECT_EQ(m_attr_num_read, 1);
  m_attr_num_read = 0;
}

TEST_F(ServerTest, Write) {
  // register our attribute
  sonar_server_register(handle_, TEST_ATTR);

  // connect (also tested by ServerTest.Connection)
  PROCESS_RECEIVE_PACKET(0x14, 0x00, 0x80);
  EXPECT_WRITE_PACKET(0x17, 0x00);
  EXPECT_TRUE(sonar_server_is_connected(handle_));
  EXPECT_EQ(m_num_connections, 1);
  m_num_connections = 0;

  // process a write request and make sure we send a response
  PROCESS_RECEIVE_PACKET(0x10, 0x01, 0xff, 0x2f, 0x40, 0x30, 0x20, 0x10);
  EXPECT_WRITE_PACKET(0x13, 0x01);
  EXPECT_EQ(m_attr_write_data, 0x10203040);
  EXPECT_EQ(m_attr_num_write, 1);
  m_attr_num_write = 0;
}

TEST_F(ServerTest, Notify) {
  // register our attribute
  sonar_server_register(handle_, TEST_ATTR);

  // connect (also tested by ServerTest.Connection)
  PROCESS_RECEIVE_PACKET(0x14, 0x00, 0x80);
  EXPECT_WRITE_PACKET(0x17, 0x00);
  EXPECT_TRUE(sonar_server_is_connected(handle_));
  EXPECT_EQ(m_num_connections, 1);
  m_num_connections = 0;

  // send a notify request
  const uint32_t data1 = 0x01020304;
  EXPECT_TRUE(sonar_server_notify(handle_, TEST_ATTR, &data1, sizeof(data1)));
  EXPECT_WRITE_PACKET(0x12, 0x80, 0xff, 0x3f, 0x04, 0x03, 0x02, 0x1);

  // time-out on receiving the response
  m_system_time += REQUEST_TIMEOUT_MS;
  sonar_server_process(handle_, NULL, 0);
  EXPECT_FALSE(m_attr_notify_complete_success);
  EXPECT_EQ(m_attr_num_notify_complete, 1);
  m_attr_num_notify_complete = 0;

  // send another notify request
  const uint32_t data2 = 0x01020304;
  EXPECT_TRUE(sonar_server_notify(handle_, TEST_ATTR, &data2, sizeof(data2)));
  EXPECT_WRITE_PACKET(0x12, 0x81, 0xff, 0x3f, 0x04, 0x03, 0x02, 0x1);

  // process the response
  PROCESS_RECEIVE_PACKET(0x11, 0x81);
  EXPECT_TRUE(m_attr_notify_complete_success);
  EXPECT_EQ(m_attr_num_notify_complete, 1);
  m_attr_num_notify_complete = 0;
}

TEST_F(ServerTest, ConcurrentReadNotify) {
  // register our attribute
  sonar_server_register(handle_, TEST_ATTR);

  // connect (also tested by ServerTest.Connection)
  PROCESS_RECEIVE_PACKET(0x14, 0x00, 0x80);
  EXPECT_WRITE_PACKET(0x17, 0x00);
  EXPECT_TRUE(sonar_server_is_connected(handle_));
  EXPECT_EQ(m_num_connections, 1);
  m_num_connections = 0;

  // send a notify request
  const uint32_t data1 = 0x01020304;
  EXPECT_TRUE(sonar_server_notify(handle_, TEST_ATTR, &data1, sizeof(data1)));
  EXPECT_WRITE_PACKET(0x12, 0x80, 0xff, 0x3f, 0x04, 0x03, 0x02, 0x1);

  // retry the notify request
  m_system_time += REQUEST_RETRY_INTERVAL_MS;
  sonar_server_process(handle_, NULL, 0);
  EXPECT_WRITE_PACKET(0x12, 0x80, 0xff, 0x3f, 0x04, 0x03, 0x02, 0x1);

  // process a read request and make sure we send a response with the correct data from the read handler
  PROCESS_RECEIVE_PACKET(0x10, 0x01, 0xff, 0x1f);
  EXPECT_WRITE_PACKET(0x13, 0x01, 0x44, 0x33, 0x22, 0x11);
  EXPECT_EQ(m_attr_num_read, 1);
  m_attr_num_read = 0;

  // retry the notify request again (should still send the original notify data and not the data we just read)
  m_system_time += REQUEST_RETRY_INTERVAL_MS;
  sonar_server_process(handle_, NULL, 0);
  EXPECT_WRITE_PACKET(0x12, 0x80, 0xff, 0x3f, 0x04, 0x03, 0x02, 0x1);

  // process the notify response
  PROCESS_RECEIVE_PACKET(0x11, 0x80);
  EXPECT_TRUE(m_attr_notify_complete_success);
  EXPECT_EQ(m_attr_num_notify_complete, 1);
  m_attr_num_notify_complete = 0;
}

TEST_F(ServerTest, NotifyReadData) {
  // register our attribute
  sonar_server_register(handle_, TEST_ATTR);

  // connect (also tested by ServerTest.Connection)
  PROCESS_RECEIVE_PACKET(0x14, 0x00, 0x80);
  EXPECT_WRITE_PACKET(0x17, 0x00);
  EXPECT_TRUE(sonar_server_is_connected(handle_));
  EXPECT_EQ(m_num_connections, 1);
  m_num_connections = 0;

  // send a notify request based on the read data
  EXPECT_TRUE(sonar_server_notify_read_data(handle_, TEST_ATTR));
  EXPECT_WRITE_PACKET(0x12, 0x80, 0xff, 0x3f, 0x44, 0x33, 0x22, 0x11);
  EXPECT_EQ(m_attr_num_read, 1);
  m_attr_num_read = 0;

  // process the notify response
  PROCESS_RECEIVE_PACKET(0x11, 0x80);
  EXPECT_TRUE(m_attr_notify_complete_success);
  EXPECT_EQ(m_attr_num_notify_complete, 1);
  m_attr_num_notify_complete = 0;
}
