#include "gtest/gtest.h"

#include "test_common.h"

extern "C" {

#include "src/common/crc16.h"
#include "src/link_layer/link_layer.h"
#include "src/link_layer/timeouts.h"

};

#define RECEIVE_HANDLE_DATA(...) do { \
    BUILD_PACKET_BUFFER(_buffer, __VA_ARGS__) \
    sonar_link_layer_handle_receive_data(handle_, _buffer, sizeof(_buffer)); \
  } while (0)

#define EXPECT_AND_CLEAR_SENT_DATA(...) do { \
    BUILD_PACKET_BUFFER(_buffer, __VA_ARGS__); \
    EXPECT_TRUE(DataMatches(m_sent_data, _buffer, sizeof(_buffer))); \
    m_sent_data.clear(); \
  } while (0)

#define EXPECT_NO_RESPONSE() do { \
    EXPECT_EQ(m_num_successful_responses, 0); \
    m_num_successful_responses = 0; \
    EXPECT_EQ(m_num_failed_responses, 0); \
    m_num_failed_responses = 0; \
  } while (0)

#define EXPECT_AND_CLEAR_RESPONSE_DATA(...) do { \
    EXPECT_EQ(m_num_successful_responses, 1); \
    m_num_successful_responses = 0; \
    EXPECT_EQ(m_num_failed_responses, 0); \
    m_num_failed_responses = 0; \
    const uint8_t _expected_data[] = {__VA_ARGS__}; \
    EXPECT_TRUE(DataMatches(m_response_data, _expected_data, sizeof(_expected_data))); \
    m_response_data.clear(); \
  } while (0)

#define SEND_REQUEST(...) do { \
    static const uint8_t _buffer[] = {__VA_ARGS__}; \
    static buffer_chain_entry_t _buffer_chain; \
    buffer_chain_set_data(&_buffer_chain, _buffer, sizeof(_buffer)); \
    EXPECT_TRUE(sonar_link_layer_send_request(handle_, &_buffer_chain)); \
  } while (0)

#define EXPECT_ERRORS(INVALID_PACKET, UNEXPECTED_PACKET, INVALID_SEQUENCE_NUMBER, RETRIES) do { \
    sonar_link_layer_errors_t _errors = {}; \
    sonar_link_layer_receive_errors_t _receive_errors = {}; \
    sonar_link_layer_get_and_clear_errors(handle_, &_errors, &_receive_errors); \
    EXPECT_EQ(_errors.invalid_packet, INVALID_PACKET); \
    EXPECT_EQ(_errors.unexpected_packet, UNEXPECTED_PACKET); \
    EXPECT_EQ(_errors.invalid_sequence_number, INVALID_SEQUENCE_NUMBER); \
    EXPECT_EQ(_errors.retries, RETRIES); \
  } while (0)

static std::vector<uint8_t> m_sent_data;
static int m_num_successful_responses = 0;
static int m_num_failed_responses = 0;
static std::vector<uint8_t> m_response_data;
static uint64_t m_system_time_ms;
static int m_num_connected_callbacks;
static int m_num_disconnected_callbacks;
static bool m_should_fail_request;

static uint64_t get_system_time_ms_function(void) {
  return m_system_time_ms;
}

static void write_byte_function(uint8_t byte) {
  m_sent_data.push_back(byte);
}

static void connection_changed_handler(void* handle, bool connected) {
  if (connected) {
    m_num_connected_callbacks++;
  } else {
    m_num_disconnected_callbacks++;
  }
}

static bool request_handler(void* handle, const uint8_t* data, uint32_t length) {
  if (m_should_fail_request) {
    return false;
  }
  // echo the data back
  static buffer_chain_entry_t response_buffer_chain;
  static uint8_t response_data[1024];
  memcpy(response_data, data, length);
  sonar_link_layer_set_response((sonar_link_layer_handle_t)handle, response_data, length);
  return true;
}

static void request_complete_handler(void* handle, bool success, const uint8_t* data, uint32_t length) {
  if (!success) {
    m_num_failed_responses++;
    return;
  }
  m_response_data.insert(m_response_data.end(), data, data + length);
  m_num_successful_responses++;
}

class LinkLayerTest : public ::testing::Test {
 protected:
  void DoLinkLayerInit(bool is_server) {
    static uint8_t receive_buffer[1024];
    static sonar_link_layer_context_t context;
    handle_ = &context;
    const sonar_link_layer_init_t init_link_layer = {
      .config = {
        .is_server = is_server,
      },
      .buffers = {
        .receive = receive_buffer,
        .receive_size = sizeof(receive_buffer),
      },
      .functions = {
        .get_system_time_ms = get_system_time_ms_function,
        .write_byte = write_byte_function,
      },
      .handlers = {
        .connection_changed = connection_changed_handler,
        .request = request_handler,
        .request_complete = request_complete_handler,
        .handler_handle = handle_,
      },
    };
    sonar_link_layer_init(handle_, &init_link_layer);
  }

  void SetUp() override {
    m_system_time_ms = 0;
    m_num_successful_responses = 0;
    m_num_failed_responses = 0;
    m_sent_data.clear();
    m_response_data.clear();
    m_num_connected_callbacks = 0;
    m_num_disconnected_callbacks = 0;
    m_should_fail_request = false;
  }

  void TearDown() override {
    EXPECT_EQ(m_num_successful_responses, 0);
    EXPECT_EQ(m_num_failed_responses, 0);
    EXPECT_TRUE(m_sent_data.empty());
    EXPECT_TRUE(m_response_data.empty());
    EXPECT_EQ(m_num_connected_callbacks, 0);
    EXPECT_EQ(m_num_disconnected_callbacks, 0);
    EXPECT_ERRORS(0, 0, 0, 0);
  }

  sonar_link_layer_handle_t handle_;
};

class LinkLayerServerTest : public LinkLayerTest {
protected:
  void SetUp() override {
    LinkLayerTest::SetUp();
    DoLinkLayerInit(true);
  }
};

class LinkLayerClientTest : public LinkLayerTest {
protected:
  void SetUp() override {
    LinkLayerTest::SetUp();
    DoLinkLayerInit(false);
  }
};

TEST_F(LinkLayerServerTest, ResponseNormal) {
  ASSERT_FALSE(sonar_link_layer_is_connected(handle_));

  // should respond to a connection request
  RECEIVE_HANDLE_DATA(0x14, 0x0b, 0x42);
  EXPECT_AND_CLEAR_SENT_DATA(0x17, 0x0b);
  EXPECT_NO_RESPONSE();
  ASSERT_TRUE(sonar_link_layer_is_connected(handle_));
  EXPECT_EQ(m_num_connected_callbacks, 1);
  m_num_connected_callbacks = 0;

  // should respond to a normal request (echo'ing data back)
  RECEIVE_HANDLE_DATA(0x10, 0x0c, 0x77);
  EXPECT_AND_CLEAR_SENT_DATA(0x13, 0x0c, 0x77);
  EXPECT_NO_RESPONSE();
}

TEST_F(LinkLayerServerTest, ResponseRetries) {
  // should respond to a connection request
  RECEIVE_HANDLE_DATA(0x14, 0x88, 0x00);
  EXPECT_AND_CLEAR_SENT_DATA(0x17, 0x88);
  EXPECT_NO_RESPONSE();
  ASSERT_TRUE(sonar_link_layer_is_connected(handle_));
  EXPECT_EQ(m_num_connected_callbacks, 1);
  m_num_connected_callbacks = 0;

  // should respond to the same connection request sent again
  RECEIVE_HANDLE_DATA(0x14, 0x88, 0x00);
  EXPECT_AND_CLEAR_SENT_DATA(0x17, 0x88);
  EXPECT_NO_RESPONSE();
  EXPECT_EQ(m_num_disconnected_callbacks, 1);
  m_num_disconnected_callbacks = 0;
  EXPECT_EQ(m_num_connected_callbacks, 1);
  m_num_connected_callbacks = 0;

  // should respond to a connection request with a very different sequence id
  RECEIVE_HANDLE_DATA(0x14, 0x0b, 0x42);
  EXPECT_AND_CLEAR_SENT_DATA(0x17, 0x0b);
  EXPECT_NO_RESPONSE();
  EXPECT_EQ(m_num_disconnected_callbacks, 1);
  m_num_disconnected_callbacks = 0;
  EXPECT_EQ(m_num_connected_callbacks, 1);
  m_num_connected_callbacks = 0;

  // should respond to that same connection request again
  RECEIVE_HANDLE_DATA(0x14, 0x0b, 0x42);
  EXPECT_AND_CLEAR_SENT_DATA(0x17, 0x0b);
  EXPECT_NO_RESPONSE();
  EXPECT_EQ(m_num_disconnected_callbacks, 1);
  m_num_disconnected_callbacks = 0;
  EXPECT_EQ(m_num_connected_callbacks, 1);
  m_num_connected_callbacks = 0;

  // should respond to a normal request (echo'ing data back)
  RECEIVE_HANDLE_DATA(0x10, 0x0c, 0x77);
  EXPECT_AND_CLEAR_SENT_DATA(0x13, 0x0c, 0x77);
  EXPECT_NO_RESPONSE();

  // should respond to the same normal request again (echo'ing data back)
  RECEIVE_HANDLE_DATA(0x10, 0x0c, 0x77);
  EXPECT_AND_CLEAR_SENT_DATA(0x13, 0x0c, 0x77);
  EXPECT_NO_RESPONSE();

  // should respond to the same normal request again (echo'ing data back)
  RECEIVE_HANDLE_DATA(0x10, 0x0c, 0x77);
  EXPECT_AND_CLEAR_SENT_DATA(0x13, 0x0c, 0x77);
  EXPECT_NO_RESPONSE();

  // send another normal request and return false from our request handler
  m_should_fail_request = true;
  RECEIVE_HANDLE_DATA(0x10, 0x0d, 0x33);
  m_should_fail_request = false;
  EXPECT_TRUE(m_sent_data.empty());
  EXPECT_NO_RESPONSE();

  // send that same request and we should drop it
  RECEIVE_HANDLE_DATA(0x10, 0x0d, 0x33);
  EXPECT_TRUE(m_sent_data.empty());
  EXPECT_NO_RESPONSE();

  // send another normal request and we should respond (echo'ing data back)
  RECEIVE_HANDLE_DATA(0x10, 0x0e, 0xf0);
  EXPECT_AND_CLEAR_SENT_DATA(0x13, 0x0e, 0xf0);
  EXPECT_NO_RESPONSE();
}

TEST_F(LinkLayerServerTest, RequestNormal) {
  // need to connect first (also covered by ServerResponse test case)
  RECEIVE_HANDLE_DATA(0x14, 0x0b, 0x42);
  EXPECT_AND_CLEAR_SENT_DATA(0x17, 0x0b);
  EXPECT_NO_RESPONSE();
  ASSERT_TRUE(sonar_link_layer_is_connected(handle_));
  EXPECT_EQ(m_num_connected_callbacks, 1);
  m_num_connected_callbacks = 0;

  // send a request with data
  SEND_REQUEST(0xaa, 0xbb, 0xcc);
  EXPECT_AND_CLEAR_SENT_DATA(0x12, 0x42, 0xaa, 0xbb, 0xcc);
  EXPECT_NO_RESPONSE();

  // process the response
  RECEIVE_HANDLE_DATA(0x11, 0x42);
  EXPECT_AND_CLEAR_RESPONSE_DATA();

  // send a request with no data
  SEND_REQUEST();
  EXPECT_AND_CLEAR_SENT_DATA(0x12, 0x43);
  EXPECT_NO_RESPONSE();

  // process the response
  RECEIVE_HANDLE_DATA(0x11, 0x43);
  EXPECT_AND_CLEAR_RESPONSE_DATA();
}

TEST_F(LinkLayerServerTest, RequestRetriesAndTimeout) {
  // need to connect first (also covered by ServerResponse test case)
  RECEIVE_HANDLE_DATA(0x14, 0x0b, 0x42);
  EXPECT_AND_CLEAR_SENT_DATA(0x17, 0x0b);
  EXPECT_NO_RESPONSE();
  ASSERT_TRUE(sonar_link_layer_is_connected(handle_));
  EXPECT_EQ(m_num_connected_callbacks, 1);
  m_num_connected_callbacks = 0;

  // increment the time by just under connection timeout and then send a connection maintenance request
  m_system_time_ms += CONNECTION_TIMEOUT_MS - 1;
  RECEIVE_HANDLE_DATA(0x14, 0x0b);
  EXPECT_AND_CLEAR_SENT_DATA(0x17, 0x0b);
  EXPECT_NO_RESPONSE();
  ASSERT_TRUE(sonar_link_layer_is_connected(handle_));
  EXPECT_EQ(m_num_connected_callbacks, 0);

  // increment the time by just under connection timeout again, make sure we didn't disconnect, and then send a connection maintenance request
  m_system_time_ms += CONNECTION_TIMEOUT_MS - 1;
  ASSERT_TRUE(sonar_link_layer_is_connected(handle_));
  EXPECT_EQ(m_num_disconnected_callbacks, 0);
  EXPECT_EQ(m_num_connected_callbacks, 0);
  RECEIVE_HANDLE_DATA(0x14, 0x0b);
  EXPECT_AND_CLEAR_SENT_DATA(0x17, 0x0b);
  EXPECT_NO_RESPONSE();
  ASSERT_TRUE(sonar_link_layer_is_connected(handle_));
  EXPECT_EQ(m_num_connected_callbacks, 0);

  // send a request with data
  SEND_REQUEST(0xaa, 0xbb, 0xcc);
  EXPECT_AND_CLEAR_SENT_DATA(0x12, 0x42, 0xaa, 0xbb, 0xcc);
  EXPECT_NO_RESPONSE();
  EXPECT_ERRORS(0, 0, 0, 0);

  // increment the time and check that we retry
  m_system_time_ms += REQUEST_RETRY_INTERVAL_MS;
  sonar_link_layer_process(handle_);
  EXPECT_AND_CLEAR_SENT_DATA(0x12, 0x42, 0xaa, 0xbb, 0xcc);
  EXPECT_NO_RESPONSE();
  EXPECT_ERRORS(0, 0, 0, 1);

  // increment the time again and check that we retry
  m_system_time_ms += REQUEST_RETRY_INTERVAL_MS;
  sonar_link_layer_process(handle_);
  EXPECT_AND_CLEAR_SENT_DATA(0x12, 0x42, 0xaa, 0xbb, 0xcc);
  EXPECT_NO_RESPONSE();
  EXPECT_ERRORS(0, 0, 0, 1);

  // increment the time by more than the timeout and check that we timed out (and didn't retry)
  m_system_time_ms += REQUEST_TIMEOUT_MS;
  sonar_link_layer_process(handle_);
  EXPECT_TRUE(m_sent_data.empty());
  m_sent_data.clear();
  EXPECT_EQ(m_num_successful_responses, 0);
  EXPECT_EQ(m_num_failed_responses, 1);
  m_num_failed_responses = 0;
  EXPECT_ERRORS(0, 0, 0, 0);

  // process the response - should be dropped since we've already timed out
  RECEIVE_HANDLE_DATA(0x11, 0x42);
  EXPECT_NO_RESPONSE();
  EXPECT_ERRORS(0, 1, 0, 0);

  // send a new request with no data
  SEND_REQUEST();
  EXPECT_AND_CLEAR_SENT_DATA(0x12, 0x43);
  EXPECT_NO_RESPONSE();

  // increment the time by more than our connection timeout and check that we disconnect and timeout the request
  m_system_time_ms += CONNECTION_TIMEOUT_MS;
  sonar_link_layer_process(handle_);
  EXPECT_TRUE(m_sent_data.empty());
  EXPECT_EQ(m_num_successful_responses, 0);
  EXPECT_EQ(m_num_failed_responses, 1);
  m_num_failed_responses = 0;
  EXPECT_FALSE(sonar_link_layer_is_connected(handle_));
  EXPECT_EQ(m_num_disconnected_callbacks, 1);
  m_num_disconnected_callbacks = 0;

  // run our process again and make sure we don't get extra callbacks (checked in TearDown())
  sonar_link_layer_process(handle_);
}

TEST_F(LinkLayerClientTest, Connection) {
  ASSERT_FALSE(sonar_link_layer_is_connected(handle_));

  // should send a connection request
  sonar_link_layer_process(handle_);
  EXPECT_AND_CLEAR_SENT_DATA(0x14, 0x01, 0x00);
  EXPECT_NO_RESPONSE();
  ASSERT_FALSE(sonar_link_layer_is_connected(handle_));
  EXPECT_EQ(m_num_connected_callbacks, 0);

  // process the response and we should be connected
  RECEIVE_HANDLE_DATA(0x17, 0x01);
  EXPECT_NO_RESPONSE();
  ASSERT_TRUE(sonar_link_layer_is_connected(handle_));
  EXPECT_EQ(m_num_connected_callbacks, 1);
  m_num_connected_callbacks = 0;

  // increment the time by the connection interval and check that we send a maintenance request
  m_system_time_ms += CONNECTION_MAINTENANCE_INTERVAL_MS;
  sonar_link_layer_process(handle_);
  EXPECT_NO_RESPONSE();
  EXPECT_AND_CLEAR_SENT_DATA(0x14, 0x02);
  RECEIVE_HANDLE_DATA(0x17, 0x02);
  ASSERT_TRUE(sonar_link_layer_is_connected(handle_));

  // increment the time by just under the connection timeout and check that we didn't disconnect and send another maintenance request
  m_system_time_ms += CONNECTION_TIMEOUT_MS - 1;
  sonar_link_layer_process(handle_);
  EXPECT_NO_RESPONSE();
  EXPECT_AND_CLEAR_SENT_DATA(0x14, 0x03);
  RECEIVE_HANDLE_DATA(0x17, 0x03);
  ASSERT_TRUE(sonar_link_layer_is_connected(handle_));

  // increment the time by the connection timeout and check that we disconnect and try to re-connect
  m_system_time_ms += CONNECTION_TIMEOUT_MS;
  sonar_link_layer_process(handle_);
  EXPECT_NO_RESPONSE();
  ASSERT_FALSE(sonar_link_layer_is_connected(handle_));
  EXPECT_AND_CLEAR_SENT_DATA(0x14, 0x04, (CONNECTION_MAINTENANCE_INTERVAL_MS + CONNECTION_TIMEOUT_MS * 2 - 1) & 0xff);
  EXPECT_EQ(m_num_disconnected_callbacks, 1);
  m_num_disconnected_callbacks = 0;
}

TEST_F(LinkLayerClientTest, ResponseNormal) {
  // need to connect first (also covered by ClientConnection test case)
  sonar_link_layer_process(handle_);
  EXPECT_AND_CLEAR_SENT_DATA(0x14, 0x01, 0x00);
  RECEIVE_HANDLE_DATA(0x17, 0x01);
  EXPECT_EQ(m_num_connected_callbacks, 1);
  m_num_connected_callbacks = 0;

  // should respond to a normal request (echo'ing data back)
  RECEIVE_HANDLE_DATA(0x12, 0x00, 0x77);
  EXPECT_AND_CLEAR_SENT_DATA(0x11, 0x00, 0x77);
  EXPECT_NO_RESPONSE();
}

TEST_F(LinkLayerClientTest, ResponseRetries) {
  // need to connect first (also covered by ClientConnection test case)
  sonar_link_layer_process(handle_);
  EXPECT_AND_CLEAR_SENT_DATA(0x14, 0x01, 0x00);
  RECEIVE_HANDLE_DATA(0x17, 0x01);
  EXPECT_EQ(m_num_connected_callbacks, 1);
  m_num_connected_callbacks = 0;

  // should respond to a normal request (echo'ing data back)
  RECEIVE_HANDLE_DATA(0x12, 0x00, 0x77);
  EXPECT_AND_CLEAR_SENT_DATA(0x11, 0x00, 0x77);
  EXPECT_NO_RESPONSE();

  // should respond to the same normal request again (echo'ing data back)
  RECEIVE_HANDLE_DATA(0x12, 0x00, 0x77);
  EXPECT_AND_CLEAR_SENT_DATA(0x11, 0x00, 0x77);
  EXPECT_NO_RESPONSE();

  // should respond to the same normal request again (echo'ing data back)
  RECEIVE_HANDLE_DATA(0x12, 0x00, 0x77);
  EXPECT_AND_CLEAR_SENT_DATA(0x11, 0x00, 0x77);
  EXPECT_NO_RESPONSE();
}

TEST_F(LinkLayerClientTest, RequestNormal) {
  // need to connect first (also covered by ClientConnection test case)
  sonar_link_layer_process(handle_);
  EXPECT_AND_CLEAR_SENT_DATA(0x14, 0x01, 0x00);
  RECEIVE_HANDLE_DATA(0x17, 0x01);
  EXPECT_EQ(m_num_connected_callbacks, 1);
  m_num_connected_callbacks = 0;

  // send a request with data
  SEND_REQUEST(0xaa, 0xbb, 0xcc);
  EXPECT_AND_CLEAR_SENT_DATA(0x10, 0x02, 0xaa, 0xbb, 0xcc);
  EXPECT_NO_RESPONSE();

  // process the response
  RECEIVE_HANDLE_DATA(0x13, 0x02);
  EXPECT_AND_CLEAR_RESPONSE_DATA();

  // send a request with no data
  SEND_REQUEST();
  EXPECT_AND_CLEAR_SENT_DATA(0x10, 0x03);
  EXPECT_NO_RESPONSE();

  // process the response
  RECEIVE_HANDLE_DATA(0x13, 0x03);
  EXPECT_AND_CLEAR_RESPONSE_DATA();
}

TEST_F(LinkLayerClientTest, RequestRetriesAndTimeout) {
  // need to connect first (also covered by ClientConnection test case)
  sonar_link_layer_process(handle_);
  EXPECT_AND_CLEAR_SENT_DATA(0x14, 0x01, 0x00);
  RECEIVE_HANDLE_DATA(0x17, 0x01);
  EXPECT_EQ(m_num_connected_callbacks, 1);
  m_num_connected_callbacks = 0;

  // send a request with data
  SEND_REQUEST(0xaa, 0xbb, 0xcc);
  EXPECT_AND_CLEAR_SENT_DATA(0x10, 0x02, 0xaa, 0xbb, 0xcc);
  EXPECT_NO_RESPONSE();
  EXPECT_ERRORS(0, 0, 0, 0);

  // increment the time and check that we retry
  m_system_time_ms += REQUEST_RETRY_INTERVAL_MS;
  sonar_link_layer_process(handle_);
  EXPECT_AND_CLEAR_SENT_DATA(0x10, 0x02, 0xaa, 0xbb, 0xcc);
  EXPECT_NO_RESPONSE();
  EXPECT_ERRORS(0, 0, 0, 1);

  // increment the time again and check that we retry
  m_system_time_ms += REQUEST_RETRY_INTERVAL_MS;
  sonar_link_layer_process(handle_);
  EXPECT_AND_CLEAR_SENT_DATA(0x10, 0x02, 0xaa, 0xbb, 0xcc);
  EXPECT_NO_RESPONSE();
  EXPECT_ERRORS(0, 0, 0, 1);

  // increment the time by more than the timeout and check that we timed out (and didn't retry)
  m_system_time_ms += REQUEST_TIMEOUT_MS;
  sonar_link_layer_process(handle_);
  EXPECT_TRUE(m_sent_data.empty());
  m_sent_data.clear();
  EXPECT_EQ(m_num_successful_responses, 0);
  EXPECT_EQ(m_num_failed_responses, 1);
  m_num_failed_responses = 0;
  EXPECT_ERRORS(0, 0, 0, 0);

  // process the response - should be dropped since we've already timed out
  RECEIVE_HANDLE_DATA(0x13, 0x02);
  EXPECT_NO_RESPONSE();
  EXPECT_ERRORS(0, 1, 0, 0);

  // send a new request with no data
  SEND_REQUEST();
  EXPECT_AND_CLEAR_SENT_DATA(0x10, 0x03);
  EXPECT_NO_RESPONSE();

  // increment the time by more than our connection timeout and check that we disconnect, timeout the request, and try to reconnect
  m_system_time_ms += CONNECTION_TIMEOUT_MS;
  sonar_link_layer_process(handle_);
  EXPECT_AND_CLEAR_SENT_DATA(0x14, 0x04, (REQUEST_RETRY_INTERVAL_MS * 2 + REQUEST_TIMEOUT_MS + CONNECTION_TIMEOUT_MS) & 0xff);
  EXPECT_EQ(m_num_successful_responses, 0);
  EXPECT_EQ(m_num_failed_responses, 1);
  m_num_failed_responses = 0;
  EXPECT_FALSE(sonar_link_layer_is_connected(handle_));
  EXPECT_EQ(m_num_disconnected_callbacks, 1);
  m_num_disconnected_callbacks = 0;
}
