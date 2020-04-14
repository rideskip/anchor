#pragma once

#include "../common/buffer_chain.h"

#include <inttypes.h>
#include <stdbool.h>

#define _SONAR_APPLICATION_LAYER_CONTEXT_SIZE ( \
    sizeof(uintptr_t) + /* pending_request_info_t.{is_active,header} */ \
    sizeof(buffer_chain_entry_t) * 2 + /* pending_request_info_t.{header_buffer_chain,data_buffer_chain} */ \
    sizeof(sonar_application_layer_init_t))

// Handle type passed to send_data_function()
typedef void* sonar_application_layer_send_data_handle_t;

// Handle type passed to attribute_*_handler()
typedef void* sonar_application_layer_attribute_handler_handle_t;

// Handle type passed to *_request_complete_handler()
typedef void* sonar_application_layer_request_complete_handler_handle_t;

typedef struct {
    // Whether or not this is the server (vs. client)
    bool is_server;
    // Function which is called to write a buffer chain over the physical link
    bool (*send_data_function)(sonar_application_layer_send_data_handle_t handle, const buffer_chain_entry_t* data);
    // Function which is called to set the response while handling a request
    void (*set_response_function)(sonar_application_layer_send_data_handle_t handle, const uint8_t* data, uint32_t length);
    // Handle passed to send_data_function()
    sonar_application_layer_send_data_handle_t send_data_handle;
    // Handler for attribute read requests
    // NOTE: This must call sonar_application_layer_read_response() with the response data
    bool (*attribute_read_handler)(sonar_application_layer_attribute_handler_handle_t handle, uint16_t attribute_id);
    // Handler for attribute write requests
    bool (*attribute_write_handler)(sonar_application_layer_attribute_handler_handle_t handle, uint16_t attribute_id, const uint8_t* data, uint32_t length);
    // Handler for attribute notify requests
    bool (*attribute_notify_handler)(sonar_application_layer_attribute_handler_handle_t handle, uint16_t attribute_id, const uint8_t* data, uint32_t length);
    // Handle passed to attribute_*_handler()
    sonar_application_layer_attribute_handler_handle_t attr_handler_handle;
    // Handler for read request completion
    void(*read_request_complete_handler)(sonar_application_layer_request_complete_handler_handle_t handle, uint16_t attribute_id, bool success, const uint8_t* data, uint32_t length);
    // Handler for write request completion
    void(*write_request_complete_handler)(sonar_application_layer_request_complete_handler_handle_t handle, uint16_t attribute_id, bool success);
    // Handler for notify request completion
    void(*notify_request_complete_handler)(sonar_application_layer_request_complete_handler_handle_t handle, uint16_t attribute_id, bool success);
    // Handle passed to *_request_complete_handler()
    sonar_application_layer_request_complete_handler_handle_t request_complete_handle;
} sonar_application_layer_init_t;

// The handle is a pointer to a pre-allocated context type (to be accessed by the SONAR implementation only)
typedef uint8_t sonar_application_layer_context_t[_SONAR_APPLICATION_LAYER_CONTEXT_SIZE];
typedef sonar_application_layer_context_t* sonar_application_layer_handle_t;

// Initializes the sonar application layer code
void sonar_application_layer_init(sonar_application_layer_handle_t handle, const sonar_application_layer_init_t* init);

// Sends a SONAR application layer read request for a given attribute, with the handler specified in sonar_application_layer_init_t being being called on completion
bool sonar_application_layer_read_request(sonar_application_layer_handle_t handle, uint16_t attribute_id);

// Sends a SONAR application layer write request for a given attribute, with the handler specified in sonar_application_layer_init_t being called on completion
// NOTE: the data pointer must remain valid until the handler is called
bool sonar_application_layer_write_request(sonar_application_layer_handle_t handle, uint16_t attribute_id, const uint8_t* data, uint32_t length);

// Sends a SONAR application layer notify request for a given attribute, with the handler specified in sonar_application_layer_init_t being being called on completion
// NOTE: the data pointer must remain valid until the handler is called
bool sonar_application_layer_notify_request(sonar_application_layer_handle_t handle, uint16_t attribute_id, const uint8_t* data, uint32_t length);

// Handles a received SONAR application layer request, populating the response as applicable
bool sonar_application_layer_handle_request(sonar_application_layer_handle_t handle, const uint8_t* data, uint32_t length);

// Handles a received SONAR application layer response
void sonar_application_layer_handle_response(sonar_application_layer_handle_t handle, bool success, const uint8_t* data, uint32_t length);

// Sends a SONAR application layer read response - should only (and must) be called from attribute_read_handler()
void sonar_application_layer_read_response(sonar_application_layer_handle_t handle, const uint8_t* data, uint32_t length);
