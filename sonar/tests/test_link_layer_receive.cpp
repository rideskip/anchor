#include "gtest/gtest.h"

#include "test_common.h"

extern "C" {

#include "src/link_layer/receive.h"

};

#define RECEIVE_BUFFER_SIZE 12

#define EXPECT_AND_CLEAR_RECEIVED_PACKET(IS_RESPONSE, IS_LINK_CONTROL, SEQUENCE_NUM, ...) do { \
    const uint8_t _expected_data[] = {__VA_ARGS__}; \
    EXPECT_EQ(m_num_received_packets, 1); \
    if (m_num_received_packets) { \
      EXPECT_EQ(m_received_is_response, IS_RESPONSE); \
      EXPECT_EQ(m_received_is_link_control, IS_LINK_CONTROL); \
      EXPECT_EQ(m_received_sequence_num, SEQUENCE_NUM); \
      EXPECT_TRUE(DataMatches(m_received_data, _expected_data, sizeof(_expected_data))); \
    } \
    m_received_data.clear(); \
    m_num_received_packets = 0; \
  } while (0)

#define RECEIVE_HANDLE_DATA(...) do { \
    BUILD_PACKET_BUFFER(_buffer, __VA_ARGS__) \
    sonar_link_layer_receive_process_data(handle_, _buffer, sizeof(_buffer)); \
  } while (0)

#define RECEIVE_HANDLE_DATA_RAW(...) do { \
    const uint8_t _buffer[] = { __VA_ARGS__ }; \
    sonar_link_layer_receive_process_data(handle_, _buffer, sizeof(_buffer)); \
  } while (0)

#define EXPECT_ERRORS(INVALID_HEADER, INVALID_CRC, BUFFER_OVERFLOW, INVALID_ESCAPE_SEQUENCE) do { \
    sonar_link_layer_receive_errors_t _errors = {}; \
    sonar_link_layer_receive_get_and_clear_errors(handle_, &_errors); \
    EXPECT_EQ(_errors.invalid_header, INVALID_HEADER); \
    EXPECT_EQ(_errors.invalid_crc, INVALID_CRC); \
    EXPECT_EQ(_errors.buffer_overflow, BUFFER_OVERFLOW); \
    EXPECT_EQ(_errors.invalid_escape_sequence, INVALID_ESCAPE_SEQUENCE); \
  } while (0)

static std::vector<uint8_t> m_received_data;
static bool m_received_is_response;
static bool m_received_is_link_control;
static uint8_t m_received_sequence_num;
static int m_num_received_packets = 0;

static void link_layer_receive_packet_handler(void* handle, bool is_response, bool is_link_control, uint8_t sequence_num, const uint8_t* data, uint32_t length) {
  m_received_is_response = is_response;
  m_received_is_link_control = is_link_control;
  m_received_sequence_num = sequence_num;
  m_received_data.insert(m_received_data.end(), data, data + length);
  m_num_received_packets++;
}

class LinkLayerReceiveTest : public ::testing::Test {
 protected:
  void DoLinkLayerReceiveInit(bool is_server) {
    static sonar_link_layer_receive_context_t context;
    static uint8_t receive_buffer[RECEIVE_BUFFER_SIZE];
    const sonar_link_layer_receive_init_t init_link_layer = {
      .is_server = is_server,
      .buffer = receive_buffer,
      .buffer_size = sizeof(receive_buffer),
      .packet_handler = link_layer_receive_packet_handler,
      .handler_handle = NULL,
    };
    handle_ = &context;
    sonar_link_layer_receive_init(handle_, &init_link_layer);
  }

  void SetUp() override {
    m_received_data.clear();
    m_num_received_packets = 0;
  }

  void TearDown() override {
    EXPECT_TRUE(m_received_data.empty());
    EXPECT_EQ(m_num_received_packets, 0);
    EXPECT_ERRORS(0, 0, 0, 0);
  }

  sonar_link_layer_receive_handle_t handle_;
};

class LinkLayerReceiveServerTest : public LinkLayerReceiveTest {
 protected:
  void SetUp() override {
    LinkLayerReceiveTest::SetUp();
    DoLinkLayerReceiveInit(true);
  }
};

class LinkLayerReceiveClientTest : public LinkLayerReceiveTest {
 protected:
  void SetUp() override {
    LinkLayerReceiveTest::SetUp();
    DoLinkLayerReceiveInit(false);
  }
};

TEST_F(LinkLayerReceiveServerTest, Valid) {
  // request, client->server, normal, no data
  RECEIVE_HANDLE_DATA(0x10, 0x0b);
  EXPECT_AND_CLEAR_RECEIVED_PACKET(false, false, 11);

  // request, client->server, normal, 1 byte of data
  RECEIVE_HANDLE_DATA(0x10, 0x0b, 0x42);
  EXPECT_AND_CLEAR_RECEIVED_PACKET(false, false, 11, 0x42);

  // request, client->server, link control, no data
  RECEIVE_HANDLE_DATA(0x14, 0x0b);
  EXPECT_AND_CLEAR_RECEIVED_PACKET(false, true, 11);

  // request, client->server, link control, 1 byte of data
  RECEIVE_HANDLE_DATA(0x14, 0x0b, 0x42);
  EXPECT_AND_CLEAR_RECEIVED_PACKET(false, true, 11, 0x42);

  // response, client->server, normal, no data
  RECEIVE_HANDLE_DATA(0x11, 0x0b);
  EXPECT_AND_CLEAR_RECEIVED_PACKET(true, false, 11);

  // response, client->server, normal, 1 byte of data
  RECEIVE_HANDLE_DATA(0x11, 0x0b, 0x42);
  EXPECT_AND_CLEAR_RECEIVED_PACKET(true, false, 11, 0x42);

  // response, client->server, link control, no data
  RECEIVE_HANDLE_DATA(0x15, 0x0b);
  EXPECT_AND_CLEAR_RECEIVED_PACKET(true, true, 11);

  // response, client->server, link control, 1 byte of data
  RECEIVE_HANDLE_DATA(0x15, 0x0b, 0x42);
  EXPECT_AND_CLEAR_RECEIVED_PACKET(true, true, 11, 0x42);
}

TEST_F(LinkLayerReceiveClientTest, Valid) {
  // request, server->client, normal, no data
  RECEIVE_HANDLE_DATA(0x12, 0x0b);
  EXPECT_AND_CLEAR_RECEIVED_PACKET(false, false, 11);

  // request, server->client, normal, 1 byte of data
  RECEIVE_HANDLE_DATA(0x12, 0x0b, 0x42);
  EXPECT_AND_CLEAR_RECEIVED_PACKET(false, false, 11, 0x42);

  // request, server->client, link control, no data
  RECEIVE_HANDLE_DATA(0x16, 0x0b);
  EXPECT_AND_CLEAR_RECEIVED_PACKET(false, true, 11);

  // request, server->client, link control, 1 byte of data
  RECEIVE_HANDLE_DATA(0x16, 0x0b, 0x42);
  EXPECT_AND_CLEAR_RECEIVED_PACKET(false, true, 11, 0x42);

  // response, server->client, normal, no data
  RECEIVE_HANDLE_DATA(0x13, 0x0b);
  EXPECT_AND_CLEAR_RECEIVED_PACKET(true, false, 11);

  // response, server->client, normal, 1 byte of data
  RECEIVE_HANDLE_DATA(0x13, 0x0b, 0x42);
  EXPECT_AND_CLEAR_RECEIVED_PACKET(true, false, 11, 0x42);

  // response, server->client, link control, no data
  RECEIVE_HANDLE_DATA(0x17, 0x0b);
  EXPECT_AND_CLEAR_RECEIVED_PACKET(true, true, 11);

  // response, server->client, link control, 1 byte of data
  RECEIVE_HANDLE_DATA(0x17, 0x0b, 0x42);
  EXPECT_AND_CLEAR_RECEIVED_PACKET(true, true, 11, 0x42);

  // response, server->client, link control, 6 bytes of data
  RECEIVE_HANDLE_DATA(0x17, 0x0b, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66);
  EXPECT_AND_CLEAR_RECEIVED_PACKET(true, true, 11, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66);

  // response, server->client, link control, 7 bytes of data
  RECEIVE_HANDLE_DATA(0x17, 0x0b, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77);
  EXPECT_AND_CLEAR_RECEIVED_PACKET(true, true, 11, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77);

  // response, server->client, link control, 8 bytes of data
  RECEIVE_HANDLE_DATA(0x17, 0x0b, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88);
  EXPECT_AND_CLEAR_RECEIVED_PACKET(true, true, 11, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88);
}

TEST_F(LinkLayerReceiveClientTest, EncodingEscaped) {
  // escaped data
  RECEIVE_HANDLE_DATA(0x7e, 0x17, 0x0b, 0x7d, 0x5e, 0x7d, 0x5d, 0x5e, 0x5d, 0xb4, 0xec, 0x7e);
  EXPECT_AND_CLEAR_RECEIVED_PACKET(true, true, 11, 0x7e, 0x7d, 0x5e, 0x5d);
  EXPECT_ERRORS(0, 0, 0, 0);
}

TEST_F(LinkLayerReceiveServerTest, InvalidCRC) {
  // request, client->server, normal, no data
  RECEIVE_HANDLE_DATA_RAW(0x7e, 0x10, 0x0b, 0x00, 0x00, 0x7e);
  EXPECT_EQ(m_num_received_packets, 0);
  EXPECT_ERRORS(0, 1, 0, 0);
}

TEST_F(LinkLayerReceiveClientTest, InvalidCRC) {
  // server->client, response, 1 byte of data
  RECEIVE_HANDLE_DATA_RAW(0x7e, 0x17, 0x0b, 0x42, 0x00, 0x00, 0x7e);
  EXPECT_EQ(m_num_received_packets, 0);
  EXPECT_ERRORS(0, 1, 0, 0);
}

TEST_F(LinkLayerReceiveServerTest, InvalidFlags) {
  // invalid version
  RECEIVE_HANDLE_DATA(0xf0, 0x0b);
  EXPECT_EQ(m_num_received_packets, 0);
  EXPECT_ERRORS(1, 0, 0, 0);

  // reserved bit
  RECEIVE_HANDLE_DATA(0x18, 0x0b);
  EXPECT_EQ(m_num_received_packets, 0);
  EXPECT_ERRORS(1, 0, 0, 0);
}

TEST_F(LinkLayerReceiveServerTest, InvalidSize) {
  // just a single flag byte
  RECEIVE_HANDLE_DATA_RAW(0x7e);
  EXPECT_EQ(m_num_received_packets, 0);
  EXPECT_ERRORS(0, 0, 0, 0);

  // 1 byte too short
  RECEIVE_HANDLE_DATA(0x10);
  EXPECT_EQ(m_num_received_packets, 0);
  EXPECT_ERRORS(0, 0, 0, 0);

  // just flag bytes
  RECEIVE_HANDLE_DATA_RAW(0x7e, 0x7e);
  EXPECT_EQ(m_num_received_packets, 0);
  EXPECT_ERRORS(0, 0, 0, 0);

  // 9 bytes of data (1 more than we can fit in the receive buffer)
  RECEIVE_HANDLE_DATA(0x17, 0x0b, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99);
  EXPECT_EQ(m_num_received_packets, 0);
  EXPECT_ERRORS(0, 0, 1, 0);
}

TEST_F(LinkLayerReceiveServerTest, MixedSequence) {
  // garbage data before the first valid packet
  RECEIVE_HANDLE_DATA_RAW(0x11, 0x22, 0x33, 0x44);
  EXPECT_EQ(m_num_received_packets, 0);
  RECEIVE_HANDLE_DATA(0x10, 0x0b);
  EXPECT_AND_CLEAR_RECEIVED_PACKET(false, false, 11);
  EXPECT_ERRORS(0, 0, 0, 0);

  // extra flag byte between valid packets
  RECEIVE_HANDLE_DATA_RAW(0x7e);
  EXPECT_EQ(m_num_received_packets, 0);
  RECEIVE_HANDLE_DATA(0x10, 0x0b);
  EXPECT_AND_CLEAR_RECEIVED_PACKET(false, false, 11);
  EXPECT_ERRORS(0, 0, 0, 0);

  // empty packet between valid packets
  RECEIVE_HANDLE_DATA_RAW(0x7e, 0x7e);
  EXPECT_EQ(m_num_received_packets, 0);
  RECEIVE_HANDLE_DATA(0x10, 0x0b);
  EXPECT_AND_CLEAR_RECEIVED_PACKET(false, false, 11);
  EXPECT_ERRORS(0, 0, 0, 0);

  // invalid packet between valid packets
  RECEIVE_HANDLE_DATA();
  EXPECT_EQ(m_num_received_packets, 0);
  RECEIVE_HANDLE_DATA(0x10, 0x0b);
  EXPECT_AND_CLEAR_RECEIVED_PACKET(false, false, 11);
  EXPECT_ERRORS(0, 0, 0, 0);
}

TEST_F(LinkLayerReceiveServerTest, DecodingError) {
  // extra escape byte before first starting flag
  RECEIVE_HANDLE_DATA_RAW(0x7d, 0x7e, 0x10, 0x0b, 0x17, 0xaf, 0x7e);
  EXPECT_AND_CLEAR_RECEIVED_PACKET(false, false, 11);
  EXPECT_ERRORS(0, 0, 0, 0);

  // extra escape byte between valid packets
  RECEIVE_HANDLE_DATA_RAW(0x7d);
  EXPECT_EQ(m_num_received_packets, 0);
  RECEIVE_HANDLE_DATA(0x10, 0x0b);
  EXPECT_AND_CLEAR_RECEIVED_PACKET(false, false, 11);
  EXPECT_ERRORS(0, 0, 0, 1);

  // multiple escape bytes between valid packets
  RECEIVE_HANDLE_DATA_RAW(0x7d, 0x7d);
  EXPECT_EQ(m_num_received_packets, 0);
  RECEIVE_HANDLE_DATA(0x10, 0x0b);
  EXPECT_AND_CLEAR_RECEIVED_PACKET(false, false, 11);
  EXPECT_ERRORS(0, 0, 0, 1);

  // invalid end flag (random byte) followed by valid packet
  RECEIVE_HANDLE_DATA_RAW(0x7e, 0x10, 0x0b, 0x17, 0xaf, 0xff);
  EXPECT_EQ(m_num_received_packets, 0);
  RECEIVE_HANDLE_DATA(0x10, 0x0b);
  EXPECT_AND_CLEAR_RECEIVED_PACKET(false, false, 11);
  EXPECT_ERRORS(0, 1, 0, 0);

  // invalid end flag (escape byte) followed by valid packet
  RECEIVE_HANDLE_DATA_RAW(0x7e, 0x10, 0x0b, 0x17, 0xaf, 0x7d);
  EXPECT_EQ(m_num_received_packets, 0);
  RECEIVE_HANDLE_DATA(0x10, 0x0b);
  EXPECT_AND_CLEAR_RECEIVED_PACKET(false, false, 11);
  EXPECT_ERRORS(0, 0, 0, 1);
}
