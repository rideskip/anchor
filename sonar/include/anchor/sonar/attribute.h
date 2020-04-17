#pragma once

#include <inttypes.h>

struct sonar_attribute_def;
typedef struct sonar_attribute_def sonar_attribute_def_t;

// The type used to represent a SONAR attribute for SONAR APIs
typedef sonar_attribute_def_t* sonar_attribute_t;

// Enum which defines various supported attribute operations
typedef enum {
    SONAR_ATTRIBUTE_OPS_R = 0x1000,
    SONAR_ATTRIBUTE_OPS_W = 0x2000,
    SONAR_ATTRIBUTE_OPS_N = 0x4000,
    SONAR_ATTRIBUTE_OPS_RW = SONAR_ATTRIBUTE_OPS_R | SONAR_ATTRIBUTE_OPS_W,
    SONAR_ATTRIBUTE_OPS_RN = SONAR_ATTRIBUTE_OPS_R | SONAR_ATTRIBUTE_OPS_N,
    SONAR_ATTRIBUTE_OPS_WN = SONAR_ATTRIBUTE_OPS_W | SONAR_ATTRIBUTE_OPS_N,
    SONAR_ATTRIBUTE_OPS_RWN = SONAR_ATTRIBUTE_OPS_R | SONAR_ATTRIBUTE_OPS_W | SONAR_ATTRIBUTE_OPS_N,
} sonar_attribute_ops_t;

struct sonar_attribute_def {
    // Allocated private context space - should only be accessed by the SONAR implementation
    uint8_t _private[sizeof(void*) * 2];
    // The ID of the attribute
    const uint16_t attribute_id:12;
    // The maximum size of the attribute data
    const uint32_t max_size;
    // Which operations the attribute supports
    const sonar_attribute_ops_t ops;
    // Pointer to a statically-allocated data buffer for the attribute which is used internally by SONAR for requests
    uint8_t* const request_buffer;
    // Pointer to a statically-allocated data buffer for the attribute which is used internally by SONAR for responses
    uint8_t* const response_buffer;
};


// SONAR_ATTR_BUFFER_ATTRIBUTES can optionally be set to add attributes to the buffers used by SONAR attributes
#ifndef SONAR_ATTR_BUFFER_ATTRIBUTES
#define SONAR_ATTR_BUFFER_ATTRIBUTES
#endif

/*
 * The SONAR_ATTR_DEF macro below is used to define SONAR attributes:
 *   NAME - The name of the variable which will be created and can be passed to sonar_server_* APIs
 *   ID - The 12-bit attribute ID
 *   MAX_SIZE - The maximum size of the attribute
 *   OPS - The operations supported by the attribute (R/W/N/RW/RN/WN/RWN)
 *
 * NOTE: MSVC doesn't allow for zero-sized buffers, so we make sure the size is 1 in those cases.
 */
#define SONAR_ATTR_DEF(NAME, ID, MAX_SIZE, OPS) \
    static uint8_t _##NAME##_request_buffer[(MAX_SIZE) ? (MAX_SIZE) : 1] SONAR_ATTR_BUFFER_ATTRIBUTES; \
    static uint8_t _##NAME##_response_buffer[(MAX_SIZE) ? (MAX_SIZE) : 1] SONAR_ATTR_BUFFER_ATTRIBUTES; \
    static sonar_attribute_def_t _##NAME##_def = { \
        ._private = {0}, \
        .attribute_id = ID, \
        .max_size = MAX_SIZE, \
        .ops = SONAR_ATTRIBUTE_OPS_##OPS, \
        .request_buffer = _##NAME##_request_buffer, \
        .response_buffer = _##NAME##_response_buffer, \
    }; \
    static const sonar_attribute_t NAME = &_##NAME##_def;
