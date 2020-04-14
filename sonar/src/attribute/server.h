#pragma once

#include "anchor/sonar/attribute.h"

#include <inttypes.h>
#include <stdbool.h>

#define _SONAR_ATTRIBUTE_SERVER_CONTEXT_SIZE \
    (sizeof(sonar_attribute_server_init_t) + sizeof(void*) + sizeof(uintptr_t) + sizeof(uint16_t) * 8)

typedef struct {
    bool (*send_notify_request_function)(void* handle, uint16_t attribute_id, const uint8_t* data, uint32_t length);
    void (*read_response_handler)(void* handle, const uint8_t* data, uint32_t length);
    uint32_t (*read_handler)(void* handle, sonar_attribute_t attr, void* response_data, uint32_t response_max_size);
    bool (*write_handler)(void* handle, sonar_attribute_t attr, const uint8_t* data, uint32_t length);
    void (*notify_complete_handler)(void* handle, bool success);
    void* handle;
} sonar_attribute_server_init_t;

// The handle is a pointer to a pre-allocated context type (to be accessed by the SONAR implementation only)
typedef uint8_t sonar_attribute_server_context_t[_SONAR_ATTRIBUTE_SERVER_CONTEXT_SIZE];
typedef sonar_attribute_server_context_t* sonar_attribute_server_handle_t;

// Initialize the SONAR attribute server code
void sonar_attribute_server_init(sonar_attribute_server_handle_t handle, const sonar_attribute_server_init_t* init);

// Register an implementation for an attribute supported by the server
void sonar_attribute_server_register(sonar_attribute_server_handle_t handle, sonar_attribute_t attribute);

// Issue a notify request for an attribute
bool sonar_attribute_server_notify(sonar_attribute_server_handle_t handle, sonar_attribute_t attribute, const uint8_t* data, uint32_t length);

// Issue a notify request for an attribute, using the data returned by calling the read handlers
bool sonar_attribute_server_notify_read_data(sonar_attribute_server_handle_t handle, sonar_attribute_t attribute);

// Handles a received attribute read reqyest
bool sonar_attribute_server_handle_read_request(sonar_attribute_server_handle_t handle, uint16_t attribute_id);

// Handles a received attribute write request
bool sonar_attribute_server_handle_write_request(sonar_attribute_server_handle_t handle, uint16_t attribute_id, const uint8_t* data, uint32_t length);

// Handles a received attribute notify response
void sonar_attribute_server_handle_notify_response(sonar_attribute_server_handle_t handle, uint16_t attribute_id, bool success);
