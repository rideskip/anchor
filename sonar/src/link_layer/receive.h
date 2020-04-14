#pragma once

#include <inttypes.h>
#include <stdbool.h>

#define _SONAR_LINK_LAYER_RECEIVE_CONTEXT_SIZE \
    (sizeof(uint32_t) * 2 + sizeof(sonar_link_layer_receive_init_t) + sizeof(sonar_link_layer_receive_errors_t))

typedef struct {
    // Whether or not this is the server (vs. client)
    bool is_server;
    // Buffer which is used internally to store incoming packets
    // Should be as big as the largest supported data segment received by the link layer (after decoding) plus 4 bytes (for the link layer header / footer)
    uint8_t* buffer;
    // Size of `buffer` in bytes
    uint32_t buffer_size;
    // Function which is called with complete SONAR link layer packets upon receipt
    void (*packet_handler)(void* handle, bool is_response, bool is_link_control, uint8_t sequence_num, const uint8_t* data, uint32_t length);
    // Handle which is passed to packet_handler()
    void* handler_handle;
} sonar_link_layer_receive_init_t;

typedef struct {
    // Invalid packets (bad header)
    uint32_t invalid_header;
    // Invalid packets (bad CRC)
    uint32_t invalid_crc;
    // Receive buffer overflows
    uint32_t buffer_overflow;
    // Invalid HDLC escape sequences
    uint32_t invalid_escape_sequence;
} sonar_link_layer_receive_errors_t;

// The handle is a pointer to a pre-allocated context type (to be accessed by the SONAR implementation only)
typedef uint8_t sonar_link_layer_receive_context_t[_SONAR_LINK_LAYER_RECEIVE_CONTEXT_SIZE];
typedef sonar_link_layer_receive_context_t* sonar_link_layer_receive_handle_t;

// Initializes the SONAR link layer receive code
void sonar_link_layer_receive_init(sonar_link_layer_receive_handle_t handle, const sonar_link_layer_receive_init_t* init);

// Processes received SONAR data
void sonar_link_layer_receive_process_data(sonar_link_layer_receive_handle_t handle, const uint8_t* data, uint32_t length);

// Get and then clear the current error counters
void sonar_link_layer_receive_get_and_clear_errors(sonar_link_layer_receive_handle_t handle, sonar_link_layer_receive_errors_t* errors);
