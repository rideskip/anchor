#include "receive.h"

#include "../common/crc16.h"
#include "types.h"

#define LOGGING_MODULE_NAME "SONAR"
#include "anchor/logging/logging.h"

#include <stdbool.h>
#include <string.h>

typedef struct {
    sonar_link_layer_receive_init_t init;
    sonar_link_layer_receive_errors_t errors;
    uint32_t received_len;
    bool packet_started;
    bool escaping;
} instance_impl_t;
_Static_assert(sizeof(sonar_link_layer_receive_context_t) == sizeof(instance_impl_t), "Invalid context size");

static void process_packet(instance_impl_t* inst) {
    if (inst->received_len < (sizeof(sonar_link_layer_header_t) + sizeof(sonar_link_layer_footer_t))) {
        return;
    }

    const uint32_t data_length = inst->received_len - sizeof(sonar_link_layer_header_t) - sizeof(sonar_link_layer_footer_t);
    const sonar_link_layer_header_t* header = (const sonar_link_layer_header_t*)inst->init.buffer;
    const sonar_link_layer_footer_t* footer = (const sonar_link_layer_footer_t*)&inst->init.buffer[sizeof(*header) + data_length];

    const uint8_t version = (header->flags & SONAR_LINK_LAYER_FLAGS_VERSION_MASK) >> SONAR_LINK_LAYER_FLAGS_VERSION_OFFSET;
    const uint16_t calculated_crc = crc16(inst->init.buffer, sizeof(sonar_link_layer_header_t) + data_length, CRC16_INITIAL_VALUE);
    const bool is_server_to_client = header->flags & SONAR_LINK_LAYER_FLAGS_DIRECTION_MASK;

    if (header->flags & SONAR_LINK_LAYER_FLAGS_RESERVED_MASK) {
        LOG_ERROR("Invalid packet: bad reserved bits");
        inst->errors.invalid_header++;
        return;
    } else if (version != SONAR_VERSION) {
        LOG_ERROR("Invalid packet: bad version");
        inst->errors.invalid_header++;
        return;
    } else if (footer->crc != calculated_crc) {
        LOG_ERROR("Invalid packet: bad CRC");
        inst->errors.invalid_crc++;
        return;
    } else if (is_server_to_client == inst->init.is_server) {
        LOG_ERROR("Invalid packet: wrong direction");
        inst->errors.invalid_header++;
        return;
    }

    const bool is_response = header->flags & SONAR_LINK_LAYER_FLAGS_RESPONSE_MASK;
    const bool is_link_control = header->flags & SONAR_LINK_LAYER_FLAGS_LINK_CONTROL_MASK;
    inst->init.packet_handler(inst->init.handler_handle, is_response, is_link_control, header->sequence_num, &inst->init.buffer[sizeof(*header)], data_length);
}

static void store_byte(instance_impl_t* inst, uint8_t byte) {
    if (inst->received_len < inst->init.buffer_size) {
        inst->init.buffer[inst->received_len++] = byte;
    } else {
        // buffer overflowed, so drop this packet and wait for the next flag byte
        LOG_ERROR("Invalid packet: overflowed buffer");
        inst->errors.buffer_overflow++;
        inst->packet_started = false;
        inst->received_len = 0;
    }
}

static void receive_byte(instance_impl_t* inst, uint8_t byte) {
    if (inst->packet_started) {
        if (inst->escaping) {
            inst->escaping = false;
            if (byte == SONAR_ENCODING_ESCAPE_BYTE || byte == SONAR_ENCODING_FLAG_BYTE) {
                // illegal escape sequence, so drop the current packet
                LOG_ERROR("Illegal escape sequence in data");
                inst->errors.invalid_escape_sequence++;
                inst->packet_started = false;
                inst->received_len = 0;
            } else {
                store_byte(inst, byte ^ SONAR_ENCODING_ESCAPE_XOR);
            }
        } else {
            if (byte == SONAR_ENCODING_ESCAPE_BYTE) {
                // escape the next byte and ignore this one
                inst->escaping = true;
            } else if (byte != SONAR_ENCODING_FLAG_BYTE) {
                store_byte(inst, byte);
            }
        }
    } else {
        inst->escaping = false;
    }

    if (byte == SONAR_ENCODING_FLAG_BYTE) {
        // a flag byte is always the end of the current packet and the start of a new packet
        process_packet(inst);
        inst->packet_started = true;
        inst->received_len = 0;
    }
}

void sonar_link_layer_receive_init(sonar_link_layer_receive_handle_t handle, const sonar_link_layer_receive_init_t* init) {
    instance_impl_t* inst = (instance_impl_t*)handle;
    *inst = (instance_impl_t){
        .init = *init,
        .packet_started = false,
        .escaping = false,
        .received_len = 0,
    };
}

void sonar_link_layer_receive_process_data(sonar_link_layer_receive_handle_t handle, const uint8_t* data, uint32_t length) {
    instance_impl_t* inst = (instance_impl_t*)handle;
    while (length--) {
        receive_byte(inst, *data++);
    }
}

void sonar_link_layer_receive_get_and_clear_errors(sonar_link_layer_receive_handle_t handle, sonar_link_layer_receive_errors_t* errors) {
    instance_impl_t* inst = (instance_impl_t*)handle;
    *errors = inst->errors;
    inst->errors = (sonar_link_layer_receive_errors_t){0};
}
