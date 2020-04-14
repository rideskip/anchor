#pragma once

#include "../common/buffer_chain.h"

#include <inttypes.h>
#include <stdbool.h>

#define _SONAR_LINK_LAYER_TRANSMIT_CONTEXT_SIZE \
    (sizeof(sonar_link_layer_transmit_init_t))

typedef struct {
    // Whether or not this is the server (vs. client)
    bool is_server;
    // Function which is called to write a byte of data over the physical link
    void (*write_byte_function)(uint8_t byte);
} sonar_link_layer_transmit_init_t;


// The handle is a pointer to a pre-allocated context type (to be accessed by the SONAR implementation only)
typedef uint8_t sonar_link_layer_transmit_context_t[_SONAR_LINK_LAYER_TRANSMIT_CONTEXT_SIZE];
typedef sonar_link_layer_transmit_context_t* sonar_link_layer_transmit_handle_t;

// Initializes the SONAR link layer transmit code
void sonar_link_layer_transmit_init(sonar_link_layer_transmit_handle_t handle, const sonar_link_layer_transmit_init_t* init);

// Transmits a SONAR link layer packet
void sonar_link_layer_transmit_send_packet(sonar_link_layer_transmit_handle_t handle, bool is_response, bool is_link_control, uint8_t sequence_num, const buffer_chain_entry_t* data);
