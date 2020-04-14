#include "gtest/gtest.h"

#include "test_common.h"

extern "C" {

#include "src/link_layer/transmit.h"

};

#define TRANSMIT_PACKET(IS_RESPONSE, IS_LINK_CONTROL, SEQUENCE_NUM, ...) do { \
    const uint8_t _buffer[] = {__VA_ARGS__}; \
    buffer_chain_entry_t _data = {}; \
    buffer_chain_set_data(&_data, _buffer, sizeof(_buffer)); \
    sonar_link_layer_transmit_send_packet(handle_, IS_RESPONSE, IS_LINK_CONTROL, SEQUENCE_NUM, &_data); \
  } while (0)

#define EXPECT_AND_CLEAR_SENT_DATA(...) do { \
    const uint8_t _expected_data[] = {__VA_ARGS__}; \
    EXPECT_TRUE(DataMatches(m_transmit_sent_data, _expected_data, sizeof(_expected_data))); \
    m_transmit_sent_data.clear(); \
  } while (0)

static std::vector<uint8_t> m_transmit_sent_data;

static void link_layer_transmit_write_byte_function(uint8_t byte) {
  m_transmit_sent_data.push_back(byte);
}

class LinkLayerTransmitTest : public ::testing::Test {
 protected:
  void DoLinkLayerTransmitInit(bool is_server) {
    static sonar_link_layer_transmit_context_t context;
    const sonar_link_layer_transmit_init_t init_link_layer = {
      .is_server = is_server,
      .write_byte_function = link_layer_transmit_write_byte_function,
    };
    handle_ = &context;
    sonar_link_layer_transmit_init(handle_, &init_link_layer);
  }

  void SetUp() override {
    m_transmit_sent_data.clear();
  }

  void TearDown() override {
    EXPECT_TRUE(m_transmit_sent_data.empty());
  }

  sonar_link_layer_transmit_handle_t handle_;
};

class LinkLayerTransmitServerTest : public LinkLayerTransmitTest {
 protected:
  void SetUp() override {
    LinkLayerTransmitTest::SetUp();
    DoLinkLayerTransmitInit(true);
  }
};

class LinkLayerTransmitClientTest : public LinkLayerTransmitTest {
 protected:
  void SetUp() override {
    LinkLayerTransmitTest::SetUp();
    DoLinkLayerTransmitInit(false);
  }
};

TEST_F(LinkLayerTransmitServerTest, Valid) {
  // request, server->client, normal, no data
  TRANSMIT_PACKET(false, false, 11);
  EXPECT_AND_CLEAR_SENT_DATA(0x7e, 0x12, 0x0b, 0x75, 0xc9, 0x7e);

  // request, server->client, normal, 1 byte of data
  TRANSMIT_PACKET(false, false, 11, 0x42);
  EXPECT_AND_CLEAR_SENT_DATA(0x7e, 0x12, 0x0b, 0x42, 0xe3, 0x55, 0x7e);

  // request, server->client, link control, no data
  TRANSMIT_PACKET(false, true, 11);
  EXPECT_AND_CLEAR_SENT_DATA(0x7e, 0x16, 0x0b, 0xb1, 0x05, 0x7e);

  // request, server->client, link control, 1 byte of data
  TRANSMIT_PACKET(false, true, 11, 0x42);
  EXPECT_AND_CLEAR_SENT_DATA(0x7e, 0x16, 0x0b, 0x42, 0x23, 0x89, 0x7e);

  // response, server->client, normal, no data
  TRANSMIT_PACKET(true, false, 11);
  EXPECT_AND_CLEAR_SENT_DATA(0x7e, 0x13, 0x0b, 0x44, 0xfa, 0x7e);

  // response, server->client, normal, 1 byte of data
  TRANSMIT_PACKET(true, false, 11, 0x42);
  EXPECT_AND_CLEAR_SENT_DATA(0x7e, 0x13, 0x0b, 0x42, 0xd3, 0x62, 0x7e);

  // response, server->client, link control, no data
  TRANSMIT_PACKET(true, true, 11);
  EXPECT_AND_CLEAR_SENT_DATA(0x7e, 0x17, 0x0b, 0x80, 0x36, 0x7e);

  // response, server->client, link control, 1 byte of data
  TRANSMIT_PACKET(true, true, 11, 0x42);
  EXPECT_AND_CLEAR_SENT_DATA(0x7e, 0x17, 0x0b, 0x42, 0x13, 0xbe, 0x7e);
}

TEST_F(LinkLayerTransmitClientTest, Valid) {
  // request, client->server, normal, no data
  TRANSMIT_PACKET(false, false, 11);
  EXPECT_AND_CLEAR_SENT_DATA(0x7e, 0x10, 0x0b, 0x17, 0xaf, 0x7e);

  // request, client->server, normal, 1 byte of data
  TRANSMIT_PACKET(false, false, 11, 0x42);
  EXPECT_AND_CLEAR_SENT_DATA(0x7e, 0x10, 0x0b, 0x42, 0x83, 0x3b, 0x7e);

  // request, client->server, link control, no data
  TRANSMIT_PACKET(false, true, 11);
  EXPECT_AND_CLEAR_SENT_DATA(0x7e, 0x14, 0x0b, 0xd3, 0x63, 0x7e);

  // request, client->server, link control, 1 byte of data
  TRANSMIT_PACKET(false, true, 11, 0x42);
  EXPECT_AND_CLEAR_SENT_DATA(0x7e, 0x14, 0x0b, 0x42, 0x43, 0xe7, 0x7e);

  // response, client->server, normal, no data
  TRANSMIT_PACKET(true, false, 11);
  EXPECT_AND_CLEAR_SENT_DATA(0x7e, 0x11, 0x0b, 0x26, 0x9c, 0x7e);

  // response, client->server, normal, 1 byte of data
  TRANSMIT_PACKET(true, false, 11, 0x42);
  EXPECT_AND_CLEAR_SENT_DATA(0x7e, 0x11, 0x0b, 0x42, 0xb3, 0x0c, 0x7e);

  // response, client->server, link control, no data
  TRANSMIT_PACKET(true, true, 11);
  EXPECT_AND_CLEAR_SENT_DATA(0x7e, 0x15, 0x0b, 0xe2, 0x50, 0x7e);

  // response, client->server, link control, 1 byte of data
  TRANSMIT_PACKET(true, true, 11, 0x42);
  EXPECT_AND_CLEAR_SENT_DATA(0x7e, 0x15, 0x0b, 0x42, 0x73, 0xd0, 0x7e);
}

TEST_F(LinkLayerTransmitClientTest, EncodingEscape) {
  // two flag data bytes
  TRANSMIT_PACKET(false, false, 11, 0x7e, 0x7e);
  EXPECT_AND_CLEAR_SENT_DATA(0x7e, 0x10, 0x0b, 0x7d, 0x5e, 0x7d, 0x5e, 0x99, 0xdb, 0x7e);

  // two flag data bytes with other data mixed in
  TRANSMIT_PACKET(false, false, 11, 0x11, 0x7e, 0x22, 0x7e, 0x33);
  EXPECT_AND_CLEAR_SENT_DATA(0x7e, 0x10, 0x0b, 0x11, 0x7d, 0x5e, 0x22, 0x7d, 0x5e, 0x33, 0xf3, 0x8e, 0x7e);
}
