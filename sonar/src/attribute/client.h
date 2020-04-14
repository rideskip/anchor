#pragma once

#include "anchor/sonar/attribute.h"

#include <inttypes.h>
#include <stdbool.h>

#define _SONAR_ATTRIBUTE_CLIENT_CONTEXT_SIZE \
    (sizeof(sonar_attribute_client_init_t) + sizeof(void*) + sizeof(uint32_t) * 2)

typedef struct {
    bool(*send_read_request_function)(void* handle, uint16_t attribute_id);
    bool(*send_write_request_function)(void* handle, uint16_t attribute_id, const uint8_t* data, uint32_t length);
    void(*connection_changed_callback)(void* handle, bool connected);
    void(*read_complete_handler)(void* handle, bool success, const uint8_t* data, uint32_t length);
    void(*write_complete_handler)(void* handle, bool success);
    bool(*notify_handler)(void* handle, sonar_attribute_t attr, const uint8_t* data, uint32_t length);
    void* handle;
} sonar_attribute_client_init_t;

// The handle is a pointer to a pre-allocated context type (to be accessed by the SONAR implementation only)
typedef uint8_t sonar_attribute_client_context_t[_SONAR_ATTRIBUTE_CLIENT_CONTEXT_SIZE];
typedef sonar_attribute_client_context_t* sonar_attribute_client_handle_t;

// Initialize the SONAR attribute client code
void sonar_attribute_client_init(sonar_attribute_client_handle_t, const sonar_attribute_client_init_t* init);

// Register an implementation for an attribute supported by the client
void sonar_attribute_client_register(sonar_attribute_client_handle_t handle, sonar_attribute_t def);

// Called when the low-level connection status changes
void sonar_attribute_client_low_level_connection_changed(sonar_attribute_client_handle_t handle, bool is_connected);

// Returns whether or not the attribute client is currently connected
bool sonar_attribute_client_is_connected(sonar_attribute_client_handle_t handle);

// Issue a read request for an attribute
bool sonar_attribute_client_read(sonar_attribute_client_handle_t handle, sonar_attribute_t attr);

// Issue a write request for an attribute
bool sonar_attribute_client_write(sonar_attribute_client_handle_t handle, sonar_attribute_t attr, const uint8_t* data, uint32_t length);

// Handles a received attribute read response
void sonar_attribute_client_handle_read_response(sonar_attribute_client_handle_t handle, uint16_t attribute_id, bool success, const uint8_t* data, uint32_t length);

// Handles a received attribute write response
void sonar_attribute_client_handle_write_response(sonar_attribute_client_handle_t handle, uint16_t attribute_id, bool success);

// Handles a received attribute notify request
bool sonar_attribute_client_handle_notify_request(sonar_attribute_client_handle_t handle, uint16_t attribute_id, const uint8_t* data, uint32_t length);
