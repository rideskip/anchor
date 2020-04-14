#pragma once

#include <inttypes.h>

typedef struct {
    struct {
        // Invalid packets (bad header)
        uint32_t invalid_header;
        // Invalid packets (bad CRC)
        uint32_t invalid_crc;
        // Receive buffer overflows
        uint32_t buffer_overflow;
        // Invalid HDLC escape sequences
        uint32_t invalid_escape_sequence;
    } link_layer_receive;
    struct {
        // Invalid packets received
        uint32_t invalid_packet;
        // Packets received when none were expected
        uint32_t unexpected_packet;
        // Packets with invalid sequence numbers
        uint32_t invalid_sequence_number;
        // Transmission retries
        uint32_t retries;
    } link_layer;
} sonar_errors_t;
