#pragma once

#include "anchor/sonar/error_types.h"
#include "anchor/sonar/attribute.h"

#include <inttypes.h>
#include <stdbool.h>

// The context size depends on whether we're compiling for a 64-bit or 32-bit system due to struct padding
#define _SONAR_CLIENT_CONTEXT_SIZE_32   340
#define _SONAR_CLIENT_CONTEXT_SIZE_64   560
#define _SONAR_CLIENT_CONTEXT_SIZE ( \
    sizeof(sonar_client_init_t) + \
    ((sizeof(uintptr_t) == 8) ? _SONAR_CLIENT_CONTEXT_SIZE_64 : _SONAR_CLIENT_CONTEXT_SIZE_32))

// Defines a SONAR client object which can support attributes of up to MAX_ATTR_SIZE
#define SONAR_CLIENT_DEF(NAME, MAX_ATTR_SIZE) \
    static uint8_t _##NAME##_receive_buffer[MAX_ATTR_SIZE + 6]; \
    static sonar_client_context_t _##NAME##_context = { \
        .receive_buffer = _##NAME##_receive_buffer, \
        .receive_buffer_size = sizeof(_##NAME##_receive_buffer), \
    }; \
    static sonar_client_handle_t NAME = &_##NAME##_context

typedef struct {
    // A function which writes a single byte over the physical layer
    void (*write_byte)(uint8_t byte);
    // A function which gets the current system time in ms
    uint64_t (*get_system_time_ms)(void);
    // Callback when the connection state changes
    void (*connection_changed_callback)(bool connected);
    // Callback when a sonar_client_read() request completes
    void (*attribute_read_complete_handler)(bool success, const void* data, uint32_t length);
    // Callback when a sonar_client_write() request completes
    void (*attribute_write_complete_handler)(bool success);
    // Callback when a notify request is received
    bool (*attribute_notify_handler)(sonar_attribute_t attr, const void* data, uint32_t length);
} sonar_client_init_t;

typedef struct {
    // Allocated space for private context to be used by the SONAR implementation only
    uint8_t _private[_SONAR_CLIENT_CONTEXT_SIZE];
    // Receive buffer used by SONAR - should be large enough to store the largest supported attribute plus 6 bytes for overhead
    uint8_t* receive_buffer;
    // The size of the receive buffer in bytes
    uint32_t receive_buffer_size;
} sonar_client_context_t;

typedef sonar_client_context_t* sonar_client_handle_t;

// Initialize the SONAR client
void sonar_client_init(sonar_client_handle_t handle, const sonar_client_init_t* init);

// The main process function for the SONAR client which gets passed data received since the last call
// This should be called regularly even if there's no received data
void sonar_client_process(sonar_client_handle_t handle, const uint8_t* received_data, uint32_t received_data_length);

// Returns whether or not a client is connected to the SONAR client
bool sonar_client_is_connected(sonar_client_handle_t handle);

// Register a SONAR client attribute which was defined with `SONAR_CLIENT_ATTR_DEF()`
void sonar_client_register(sonar_client_handle_t handle, sonar_attribute_t attr);

// Sends a read request for the specified attribute
bool sonar_client_read(sonar_client_handle_t handle, sonar_attribute_t attr);

// Sends a write request for the specified attribute
bool sonar_client_write(sonar_client_handle_t handle, sonar_attribute_t attr, const void* data, uint32_t length);

// Gets the error counters and then clears them
void sonar_client_get_and_clear_errors(sonar_client_handle_t handle, sonar_errors_t* errors);
