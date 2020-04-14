#pragma once

#include "anchor/sonar/error_types.h"
#include "anchor/sonar/attribute.h"

#include <inttypes.h>
#include <stdbool.h>

// The context size depends on whether we're compiling for a 64-bit or 32-bit system due to struct padding
// TODO: haven't figured out the correct 32-bit value yet
#define _SONAR_SERVER_CONTEXT_SIZE_32   352
#define _SONAR_SERVER_CONTEXT_SIZE_64   576
#define _SONAR_SERVER_CONTEXT_SIZE ( \
    sizeof(sonar_server_init_t) + \
    ((sizeof(uintptr_t) == 8) ? _SONAR_SERVER_CONTEXT_SIZE_64 : _SONAR_SERVER_CONTEXT_SIZE_32))

// Defines a SONAR server object which can support attributes of up to MAX_ATTR_SIZE
#define SONAR_SERVER_DEF(NAME, MAX_ATTR_SIZE) \
    static uint8_t _##NAME##_receive_buffer[MAX_ATTR_SIZE + 6 /* protocol overhead */]; \
    static struct sonar_server_context _##NAME##_context = { \
        .receive_buffer = _##NAME##_receive_buffer, \
        .receive_buffer_size = sizeof(_##NAME##_receive_buffer), \
    }; \
    static sonar_server_handle_t NAME = &_##NAME##_context;

// Defines a SONAR server attribute object
#define SONAR_SERVER_ATTR_DEF(ATTR_NAME, VAR_NAME, ID, MAX_SIZE, OPS) \
    _SONAR_SERVER_ATTR_HANDLERS_##OPS(ATTR_NAME) \
    SONAR_SERVER_ATTR_DEF_NO_PROTOTYPES(ATTR_NAME, VAR_NAME, ID, MAX_SIZE, OPS)
#define SONAR_SERVER_ATTR_DEF_NO_PROTOTYPES(ATTR_NAME, VAR_NAME, ID, MAX_SIZE, OPS) \
    SONAR_ATTR_DEF(_##VAR_NAME##_attr, ID, MAX_SIZE, OPS); \
    static struct sonar_server_attribute _##VAR_NAME##_server_attr = { \
        .attr = _##VAR_NAME##_attr, \
        .read_handler = ATTR_NAME##_read_handler, \
        .write_handler = ATTR_NAME##_write_handler, \
    }; \
    static sonar_server_attribute_t VAR_NAME = &_##VAR_NAME##_server_attr

// Helper macros for SONAR_SERVER_ATTR_DEF()
#define _SONAR_SERVER_ATTR_HANDLERS_R(NAME) \
    static uint32_t NAME##_read_handler(void* response_data, uint32_t response_max_size); \
    static const void* const NAME##_write_handler = NULL;
#define _SONAR_SERVER_ATTR_HANDLERS_W(NAME) \
    static const void* const NAME##_read_handler = NULL; \
    static bool NAME##_write_handler(const void* data, uint32_t length);
#define _SONAR_SERVER_ATTR_HANDLERS_N(NAME) \
    static const void* const NAME##_read_handler = NULL; \
    static const void* const NAME##_write_handler = NULL;
#define _SONAR_SERVER_ATTR_HANDLERS_RW(NAME) \
    static uint32_t NAME##_read_handler(void* response_data, uint32_t response_max_size); \
    static bool NAME##_write_handler(const void* data, uint32_t length);
#define _SONAR_SERVER_ATTR_HANDLERS_RN(NAME) _SONAR_SERVER_ATTR_HANDLERS_R(NAME)
#define _SONAR_SERVER_ATTR_HANDLERS_WN(NAME) _SONAR_SERVER_ATTR_HANDLERS_W(NAME)
#define _SONAR_SERVER_ATTR_HANDLERS_RWN(NAME) _SONAR_SERVER_ATTR_HANDLERS_RW(NAME)

// forward-declare some types
struct sonar_server_context;
typedef struct sonar_server_context* sonar_server_handle_t;
struct sonar_server_attribute;
typedef struct sonar_server_attribute* sonar_server_attribute_t;

typedef struct {
    // A function which writes a single byte over the physical layer
    void (*write_byte)(uint8_t byte);
    // A function which gets the current system time in ms
    uint64_t (*get_system_time_ms)(void);
    // Callback when the connection state changes
    void (*connection_changed_callback)(sonar_server_handle_t handle, bool connected);
    // Attribute notify complete handler
    void (*attribute_notify_complete_handler)(sonar_server_handle_t handle, bool success);
} sonar_server_init_t;

// Function prototype for attribute read handlers
typedef uint32_t (*sonar_server_attribute_read_handler_t)(void* response_data, uint32_t response_max_size);

// Function prototype for attribute write handlers
typedef bool (*sonar_server_attribute_write_handler_t)(const void* data, uint32_t length);

// A wrapper around an attribute for use by a server
struct sonar_server_attribute {
    // Allocated space for private context to be used by the SONAR server implementation only
    uint8_t _private[sizeof(void*)];
    // The SONAR attribute
    sonar_attribute_t attr;
    // Read handler for the attribute
    sonar_server_attribute_read_handler_t read_handler;
    // Write handler for the attribute
    sonar_server_attribute_write_handler_t write_handler;
};

struct sonar_server_context {
    // Allocated space for private context to be used by the SONAR server implementation only
    uint8_t _private[_SONAR_SERVER_CONTEXT_SIZE];
    // Receive buffer used by SONAR - should be large enough to store the largest supported attribute plus 6 bytes for overhead
    uint8_t* receive_buffer;
    // The size of the receive buffer in bytes
    uint32_t receive_buffer_size;
};

// Initialize the SONAR server
void sonar_server_init(sonar_server_handle_t handle, const sonar_server_init_t* init);

// The main process function for the SONAR server which gets passed data received since the last call
// This should be called regularly even if there's no received data
void sonar_server_process(sonar_server_handle_t handle, const uint8_t* received_data, uint32_t received_data_length);

// Returns whether or not a client is connected to the SONAR server
bool sonar_server_is_connected(sonar_server_handle_t handle);

// Function to register a SONAR server attribute which was defined with `SONAR_SERVER_ATTR_DEF()`
void sonar_server_register(sonar_server_handle_t handle, sonar_server_attribute_t attr);

// Sends a notify request for the specified attribute
// NOTE: the data passed to this function must remain valid until attribute_notify_complete_handler() is called
bool sonar_server_notify(sonar_server_handle_t handle, sonar_server_attribute_t attr, const void* data, uint32_t length);

// Sends a notify request for the specified attribute based on the data returned by the attribute_read_handler()
bool sonar_server_notify_read_data(sonar_server_handle_t handle, sonar_server_attribute_t attr);

// Gets the error counters and then clears them
void sonar_server_get_and_clear_errors(sonar_server_handle_t handle, sonar_errors_t* errors);
