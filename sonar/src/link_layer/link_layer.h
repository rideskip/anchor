#pragma once

#include "receive.h"
#include "transmit.h"
#include "../common/buffer_chain.h"

#include <inttypes.h>
#include <stdbool.h>

#define _SONAR_LINK_LAYER_CONTEXT_SIZE ( \
    sizeof(sonar_link_layer_init_t) + \
    sizeof(sonar_link_layer_errors_t) + \
    sizeof(sonar_link_layer_receive_context_t) + \
    sizeof(sonar_link_layer_transmit_context_t) + \
    sizeof(sonar_link_layer_receive_handle_t) + \
    sizeof(sonar_link_layer_transmit_handle_t) + \
    sizeof(buffer_chain_entry_t) + \
    sizeof(uint64_t) * 2 + \
    sizeof(uint64_t) * 4 + \
    sizeof(uintptr_t) + sizeof(uint64_t) * 2 + sizeof(void*) + \
    sizeof(uint32_t) * 2 + sizeof(void*) + \
    sizeof(uintptr_t))

typedef struct {
    struct {
        // Whether or not this is the server (vs. client)
        bool is_server;
    } config;
    struct {
        // Buffer used to receive data into by the link layer receive code
        // Should be as big as the largest supported data segment received by the link layer (after decoding) plus 4 bytes (for the link layer header / footer)
        uint8_t* receive;
        // Size of the `receive` buffer in bytes
        uint32_t receive_size;
    } buffers;
    struct {
        // Function which returns the current system time in ms
        uint64_t (*get_system_time_ms)(void);
        // Function which is called to write data over the physical link
        void (*write_byte)(uint8_t byte);
    } functions;
    struct {
        // Function which is called when the connection state changes
        void (*connection_changed)(void* handle, bool connected);
        // Function which is called when a request is received
        // This function should return false on error or return true and populate the response as appropriate
        // NOTE: On success, this callback MUST call sonar_link_layer_set_response() with the response data (or NULL/0)
        bool (*request)(void* handle, const uint8_t* data, uint32_t length);
        // Function which is called when a request (issued via sonar_link_layer_send_request()) completes
        void (*request_complete)(void* handle, bool success, const uint8_t* data, uint32_t length);
        // Handle which is passed to the handlers
        void* handler_handle;
    } handlers;
} sonar_link_layer_init_t;

typedef struct {
    // Invalid packets received
    uint32_t invalid_packet;
    // Packets received when none were expected
    uint32_t unexpected_packet;
    // Packets with invalid sequence numbers
    uint32_t invalid_sequence_number;
    // Transmission retries
    uint32_t retries;
} sonar_link_layer_errors_t;

// The handle is a pointer to a pre-allocated context type (to be accessed by the SONAR implementation only)
typedef uint8_t sonar_link_layer_context_t[_SONAR_LINK_LAYER_CONTEXT_SIZE];
typedef sonar_link_layer_context_t* sonar_link_layer_handle_t;

// Initializes the link layer
void sonar_link_layer_init(sonar_link_layer_handle_t handle, const sonar_link_layer_init_t* init);

// Returns whether or not we're currently connected
bool sonar_link_layer_is_connected(sonar_link_layer_handle_t handle);

// Processes received data
void sonar_link_layer_handle_receive_data(sonar_link_layer_handle_t handle, const uint8_t* data, uint32_t length);

// NOTE: If this returns true, `data` must remain valid and stable until the request_complete() callback is called
bool sonar_link_layer_send_request(sonar_link_layer_handle_t handle, const buffer_chain_entry_t* data);

// Run link layer processing (should be called regularly - ideally at least every 1ms)
void sonar_link_layer_process(sonar_link_layer_handle_t handle);

// Sets the SONAR link layer response - should only (and must) be called from handlers.request()
void sonar_link_layer_set_response(sonar_link_layer_handle_t handle, const uint8_t* data, uint32_t length);

// Get and then clear the current error counters
void sonar_link_layer_get_and_clear_errors(sonar_link_layer_handle_t handle, sonar_link_layer_errors_t* errors, sonar_link_layer_receive_errors_t* receive_errors);
