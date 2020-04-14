#include "transmit.h"

#include "../common/crc16.h"
#include "types.h"

typedef struct {
    sonar_link_layer_transmit_init_t init;
} instance_impl_t;
_Static_assert(sizeof(instance_impl_t) == sizeof(sonar_link_layer_transmit_context_t), "Invalid context size");

static void write_encoded_bytes(instance_impl_t* inst, const uint8_t* data, uint32_t length) {
    for (uint32_t i = 0; i < length; i++) {
        uint8_t byte = data[i];
        if (byte == SONAR_ENCODING_FLAG_BYTE || byte == SONAR_ENCODING_ESCAPE_BYTE) {
            inst->init.write_byte_function(SONAR_ENCODING_ESCAPE_BYTE);
            byte ^= SONAR_ENCODING_ESCAPE_XOR;
        }
        inst->init.write_byte_function(byte);
    }
}

void sonar_link_layer_transmit_init(sonar_link_layer_transmit_handle_t handle, const sonar_link_layer_transmit_init_t* init) {
    instance_impl_t* inst = (instance_impl_t*)handle;
    *inst = (instance_impl_t){
        .init = *init,
    };
}

void sonar_link_layer_transmit_send_packet(sonar_link_layer_transmit_handle_t handle, bool is_response, bool is_link_control, uint8_t sequence_num, const buffer_chain_entry_t* data) {
    instance_impl_t* inst = (instance_impl_t*)handle;
    uint16_t crc = CRC16_INITIAL_VALUE;

    // write the starting flag byte
    inst->init.write_byte_function(SONAR_ENCODING_FLAG_BYTE);

    // write the header
    const sonar_link_layer_header_t header = {
        .flags = (SONAR_VERSION << SONAR_LINK_LAYER_FLAGS_VERSION_OFFSET) |
            (is_link_control ? SONAR_LINK_LAYER_FLAGS_LINK_CONTROL_MASK : 0) |
            (inst->init.is_server ? SONAR_LINK_LAYER_FLAGS_DIRECTION_MASK : 0) |
            (is_response ? SONAR_LINK_LAYER_FLAGS_RESPONSE_MASK : 0),
        .sequence_num = sequence_num,
    };
    write_encoded_bytes(inst, (const uint8_t*)&header, sizeof(header));
    crc = crc16((const uint8_t*)&header, sizeof(header), crc);

    // write the data
    FOREACH_BUFFER_CHAIN_ENTRY(data, entry) {
        write_encoded_bytes(inst, entry->data, entry->length);
        crc = crc16(entry->data, entry->length, crc);
    }

    // write the footer
    const sonar_link_layer_footer_t footer = {
        .crc = crc,
    };
    write_encoded_bytes(inst, (const uint8_t*)&footer, sizeof(footer));

    // write the ending flag byte
    inst->init.write_byte_function(SONAR_ENCODING_FLAG_BYTE);
}
