#include "gtest/gtest.h"

#include "test_common.h"

extern "C" {

#include "src/application_layer/application_layer.h"

};

#define EXPECT_AND_CLEAR_SENT_PACKET(ATTR_ID, ...) do { \
    EXPECT_EQ(m_num_sent_packets, 1); \
    m_num_sent_packets = 0; \
    const uint8_t _expected_data[] = {(ATTR_ID) & 0xff, (ATTR_ID) >> 8, __VA_ARGS__}; \
    EXPECT_TRUE(DataMatches(m_sent_data, _expected_data, sizeof(_expected_data))); \
    m_sent_data.clear(); \
  } while (0)

#define SEND_READ_REQUEST(ATTR_ID) do { \
    EXPECT_TRUE(sonar_application_layer_read_request(handle_, ATTR_ID)); \
  } while (0)

#define SEND_WRITE_REQUEST(ATTR_ID, ...) do { \
    const uint8_t _buffer[] = {__VA_ARGS__}; \
    EXPECT_TRUE(sonar_application_layer_write_request(handle_, ATTR_ID, _buffer, sizeof(_buffer))); \
  } while (0)

#define SEND_NOTIFY_REQUEST(ATTR_ID, ...) do { \
    const uint8_t _buffer[] = {__VA_ARGS__}; \
    EXPECT_TRUE(sonar_application_layer_notify_request(handle_, ATTR_ID, _buffer, sizeof(_buffer))); \
  } while (0)

#define HANDLE_REQUEST_DATA_NO_RESPONSE(...) do { \
    static const uint8_t _request[] = { __VA_ARGS__ }; \
    EXPECT_TRUE(sonar_application_layer_handle_request(handle_, _request, sizeof(_request))); \
    EXPECT_TRUE(m_response_data.empty()); \
  } while (0)

#define HANDLE_REQUEST_NO_DATA_WITH_RESPONSE(ATTR_ID_L, ATTR_ID_H, ...) do { \
    static const uint8_t _request[] = { ATTR_ID_L, ATTR_ID_H }; \
    const uint8_t _expected_response[] = { __VA_ARGS__ }; \
    m_response_length = sizeof(_expected_response); \
    EXPECT_TRUE(sonar_application_layer_handle_request(handle_, _request, sizeof(_request))); \
    EXPECT_TRUE(DataMatches(std::vector<uint8_t>(&_expected_response[0], _expected_response + sizeof(_expected_response)), m_response_data.data(), m_response_data.size())); \
    m_response_data.clear(); \
  } while (0)

#define EXPECT_READ_REQUEST(ATTR_ID, ...) do { \
    EXPECT_EQ(m_request_attribute_id, ATTR_ID); \
    EXPECT_TRUE(m_request_data.empty()); \
    EXPECT_EQ(m_num_read_requests, 1); \
    EXPECT_EQ(m_num_write_requests, 0); \
    EXPECT_EQ(m_num_notify_requests, 0); \
    m_num_read_requests = 0; \
    m_num_write_requests = 0; \
    m_num_notify_requests = 0; \
    m_request_data.clear(); \
  } while (0)

#define EXPECT_WRITE_REQUEST(ATTR_ID, ...) do { \
    EXPECT_EQ(m_request_attribute_id, ATTR_ID); \
    const uint8_t _expected_data[] = {__VA_ARGS__}; \
    EXPECT_TRUE(DataMatches(m_request_data, _expected_data, sizeof(_expected_data))); \
    EXPECT_EQ(m_num_read_requests, 0); \
    EXPECT_EQ(m_num_write_requests, 1); \
    EXPECT_EQ(m_num_notify_requests, 0); \
    m_num_read_requests = 0; \
    m_num_write_requests = 0; \
    m_num_notify_requests = 0; \
    m_request_data.clear(); \
  } while (0)

#define EXPECT_NOTIFY_REQUEST(ATTR_ID, ...) do { \
    EXPECT_EQ(m_request_attribute_id, ATTR_ID); \
    const uint8_t _expected_data[] = {__VA_ARGS__}; \
    EXPECT_TRUE(DataMatches(m_request_data, _expected_data, sizeof(_expected_data))); \
    EXPECT_EQ(m_num_read_requests, 0); \
    EXPECT_EQ(m_num_write_requests, 0); \
    EXPECT_EQ(m_num_notify_requests, 1); \
    m_num_read_requests = 0; \
    m_num_write_requests = 0; \
    m_num_notify_requests = 0; \
    m_request_data.clear(); \
  } while (0)

#define HANDLE_RESPONSE(SUCCESS, ...) do { \
  const uint8_t _buffer[] = {__VA_ARGS__}; \
  sonar_application_layer_handle_response(handle_, SUCCESS, _buffer, sizeof(_buffer)); \
  } while (0)

#define EXPECT_READ_COMPLETE(ATTR_ID, SUCCESS, ...) do { \
    EXPECT_EQ(m_num_read_complete, 1); \
    EXPECT_EQ(m_num_write_complete, 0); \
    EXPECT_EQ(m_num_notify_complete, 0); \
    EXPECT_EQ(m_complete_attribute_id, ATTR_ID); \
    EXPECT_EQ(m_complete_success, SUCCESS); \
    const uint8_t _data[] = {__VA_ARGS__}; \
    EXPECT_TRUE(DataMatches(m_complete_data, _data, sizeof(_data))); \
    m_num_read_complete = 0; \
    m_num_write_complete = 0; \
    m_num_notify_complete = 0; \
    m_complete_data.clear(); \
  } while (0)

#define EXPECT_WRITE_COMPLETE(ATTR_ID, SUCCESS) do { \
    EXPECT_EQ(m_num_read_complete, 0); \
    EXPECT_EQ(m_num_write_complete, 1); \
    EXPECT_EQ(m_num_notify_complete, 0); \
    EXPECT_EQ(m_complete_attribute_id, ATTR_ID); \
    EXPECT_EQ(m_complete_success, SUCCESS); \
    EXPECT_TRUE(m_complete_data.empty()); \
    m_num_read_complete = 0; \
    m_num_write_complete = 0; \
    m_num_notify_complete = 0; \
    m_complete_data.clear(); \
  } while (0)

#define EXPECT_NOTIFY_COMPLETE(ATTR_ID, SUCCESS) do { \
    EXPECT_EQ(m_num_read_complete, 0); \
    EXPECT_EQ(m_num_write_complete, 0); \
    EXPECT_EQ(m_num_notify_complete, 1); \
    EXPECT_EQ(m_complete_attribute_id, ATTR_ID); \
    EXPECT_EQ(m_complete_success, SUCCESS); \
    EXPECT_TRUE(m_complete_data.empty()); \
    m_num_read_complete = 0; \
    m_num_write_complete = 0; \
    m_num_notify_complete = 0; \
    m_complete_data.clear(); \
  } while (0)

static int m_num_sent_packets;
static std::vector<uint8_t> m_sent_data;
static int m_num_read_requests;
static int m_num_write_requests;
static int m_num_notify_requests;
static uint16_t m_request_attribute_id;
static std::vector<uint8_t> m_request_data;
static int m_num_read_complete;
static int m_num_write_complete;
static int m_num_notify_complete;
static bool m_complete_success;
static uint16_t m_complete_attribute_id;;
static std::vector<uint8_t> m_complete_data;
static std::vector<uint8_t> m_response_data;
static uint32_t m_response_length;

static bool send_data_function(void* handle, const buffer_chain_entry_t* data) {
  m_num_sent_packets++;
  std::vector<uint8_t> new_data = buffer_chain_to_vector(data);
  m_sent_data.insert(m_sent_data.end(), new_data.begin(), new_data.end());
  return true;
}

static void set_response_function(void* handle, const uint8_t* data, uint32_t length) {
  m_response_data.insert(m_response_data.end(), data, data + length);
}

static bool attribute_read_handler(void* handle, uint16_t attribute_id) {
  static uint8_t response_data[1024];
  for (uint32_t i = 0; i < sizeof(response_data); i++) {
    response_data[i] = i & 0xff;
  }
  if (m_response_length > sizeof(response_data)) {
    return false;
  }
  sonar_application_layer_read_response((sonar_application_layer_handle_t)handle, response_data, m_response_length);
  m_num_read_requests++;
  m_request_attribute_id = attribute_id;
  return true;
}

static bool attribute_write_handler(void* handle, uint16_t attribute_id, const uint8_t* data, uint32_t length) {
  m_num_write_requests++;
  m_request_attribute_id = attribute_id;
  m_request_data.insert(m_request_data.end(), data, data + length);
  return true;
}

static bool attribute_notify_handler(void* handle, uint16_t attribute_id, const uint8_t* data, uint32_t length) {
  m_num_notify_requests++;
  m_request_attribute_id = attribute_id;
  m_request_data.insert(m_request_data.end(), data, data + length);
  return true;
}

static void read_request_complete_handler(void* handle, uint16_t attribute_id, bool success, const uint8_t* data, uint32_t length) {
  m_num_read_complete++;
  m_complete_attribute_id = attribute_id;
  m_complete_success = success;
  m_complete_data.insert(m_complete_data.end(), data, data + length);
}

static void write_request_complete_handler(void* handle, uint16_t attribute_id, bool success) {
  m_num_write_complete++;
  m_complete_attribute_id = attribute_id;
  m_complete_success = success;
}

static void notify_request_complete_handler(void* handle, uint16_t attribute_id, bool success) {
  m_num_notify_complete++;
  m_complete_attribute_id = attribute_id;
  m_complete_success = success;
}

class ApplicationLayerTest : public ::testing::Test {
 protected:
  void DoApplicationLayerInit(bool is_server) {
    static sonar_application_layer_context_t context;
    handle_ = &context;
    const sonar_application_layer_init_t init_application_layer = {
      .is_server = is_server,
      .send_data_function = send_data_function,
      .set_response_function = set_response_function,
      .send_data_handle = NULL,
      .attribute_read_handler = attribute_read_handler,
      .attribute_write_handler = attribute_write_handler,
      .attribute_notify_handler = attribute_notify_handler,
      .attr_handler_handle = handle_,

      .read_request_complete_handler = read_request_complete_handler,
      .write_request_complete_handler = write_request_complete_handler,
      .notify_request_complete_handler = notify_request_complete_handler,
      .request_complete_handle = NULL,
    };
    sonar_application_layer_init(handle_, &init_application_layer);
  }

  void SetUp() override {
    m_num_sent_packets = 0;
    m_sent_data.clear();
    m_num_read_requests = 0;
    m_num_write_requests = 0;
    m_num_notify_requests = 0;
    m_request_data.clear();
    m_num_read_complete = 0;
    m_num_write_complete = 0;
    m_num_notify_complete = 0;
    m_complete_success = false;
    m_complete_attribute_id = 0;
    m_complete_data.clear();
  }

  void TearDown() override {
    EXPECT_EQ(m_num_sent_packets, 0);
    EXPECT_TRUE(m_sent_data.empty());
    EXPECT_EQ(m_num_read_requests, 0);
    EXPECT_EQ(m_num_write_requests, 0);
    EXPECT_EQ(m_num_notify_requests, 0);
    EXPECT_EQ(m_num_read_complete, 0);
    EXPECT_EQ(m_num_write_complete, 0);
    EXPECT_EQ(m_num_notify_complete, 0);
    EXPECT_TRUE(m_complete_data.empty());
  }

  sonar_application_layer_handle_t handle_;
};

class ApplicationLayerServerTest : public ApplicationLayerTest {
 protected:
  void SetUp() override {
    ApplicationLayerTest::SetUp();
    DoApplicationLayerInit(true);
  }
};

class ApplicationLayerClientTest : public ApplicationLayerTest {
 protected:
  void SetUp() override {
    ApplicationLayerTest::SetUp();
    DoApplicationLayerInit(false);
  }
};

TEST_F(ApplicationLayerClientTest, SendReadRequest) {
  // request
  SEND_READ_REQUEST(0xabc);
  EXPECT_AND_CLEAR_SENT_PACKET(0x1abc);

  // response with 2 data bytes
  HANDLE_RESPONSE(true, 0xf1, 0xf2);
  EXPECT_READ_COMPLETE(0xabc, true, 0xf1, 0xf2);
}

TEST_F(ApplicationLayerClientTest, SendWriteRequest) {
  // request with no data
  SEND_WRITE_REQUEST(0xabc);
  EXPECT_AND_CLEAR_SENT_PACKET(0x2abc);

  // response
  HANDLE_RESPONSE(true);
  EXPECT_WRITE_COMPLETE(0xabc, true);

  // request with 2 data bytes
  SEND_WRITE_REQUEST(0xabc, 0xff, 0xee);
  EXPECT_AND_CLEAR_SENT_PACKET(0x2abc, 0xff, 0xee);

  // response
  HANDLE_RESPONSE(true);
  EXPECT_WRITE_COMPLETE(0xabc, true);
}

TEST_F(ApplicationLayerClientTest, HandleNotifyRequest) {
  // no data
  HANDLE_REQUEST_DATA_NO_RESPONSE(0xbc, 0x3a);
  EXPECT_NOTIFY_REQUEST(0xabc);

  // 2 data bytes
  HANDLE_REQUEST_DATA_NO_RESPONSE(0xbc, 0x3a, 0x11, 0x22);
  EXPECT_NOTIFY_REQUEST(0xabc, 0x11, 0x22);
}

TEST_F(ApplicationLayerServerTest, SendNotifyRequest) {
  // request with no data
  SEND_NOTIFY_REQUEST(0xabc);
  EXPECT_AND_CLEAR_SENT_PACKET(0x3abc);

  // response
  HANDLE_RESPONSE(true);
  EXPECT_NOTIFY_COMPLETE(0xabc, true);

  // request with 2 data bytes
  SEND_NOTIFY_REQUEST(0xabc, 0xff, 0xee);
  EXPECT_AND_CLEAR_SENT_PACKET(0x3abc, 0xff, 0xee);

  // response
  HANDLE_RESPONSE(true);
  EXPECT_NOTIFY_COMPLETE(0xabc, true);
}

TEST_F(ApplicationLayerServerTest, HandleReadRequest) {
  // no data
  HANDLE_REQUEST_NO_DATA_WITH_RESPONSE(0xbc, 0x1a);
  EXPECT_READ_REQUEST(0xabc);

  // 2 data bytes
  HANDLE_REQUEST_NO_DATA_WITH_RESPONSE(0xbc, 0x1a, 0x00, 0x01);
  EXPECT_READ_REQUEST(0xabc, 0x00, 0x01);
}

TEST_F(ApplicationLayerServerTest, HandleWriteRequest) {
  // no data
  HANDLE_REQUEST_DATA_NO_RESPONSE(0xbc, 0x2a);
  EXPECT_WRITE_REQUEST(0xabc);

  // 2 data bytes
  HANDLE_REQUEST_DATA_NO_RESPONSE(0xbc, 0x2a, 0x11, 0x22);
  EXPECT_WRITE_REQUEST(0xabc, 0x11, 0x22);
}
