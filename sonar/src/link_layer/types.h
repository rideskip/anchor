#pragma once

#include <inttypes.h>
#include <stdbool.h>

#define SONAR_VERSION                                   1

#define SONAR_ENCODING_FLAG_BYTE                        0x7E
#define SONAR_ENCODING_ESCAPE_BYTE                      0x7D
#define SONAR_ENCODING_ESCAPE_XOR                       0x20

#define SONAR_LINK_LAYER_FLAGS_RESPONSE_MASK            (1 << 0)
#define SONAR_LINK_LAYER_FLAGS_DIRECTION_MASK           (1 << 1)
#define SONAR_LINK_LAYER_FLAGS_LINK_CONTROL_MASK        (1 << 2)
#define SONAR_LINK_LAYER_FLAGS_RESERVED_MASK            (1 << 3)
#define SONAR_LINK_LAYER_FLAGS_VERSION_MASK             0xf0
#define SONAR_LINK_LAYER_FLAGS_VERSION_OFFSET           4

#pragma pack(push, 1)

typedef struct {
    uint8_t flags;
    uint8_t sequence_num;
} sonar_link_layer_header_t;

typedef struct {
    uint16_t crc;
} sonar_link_layer_footer_t;

#pragma pack(pop)
