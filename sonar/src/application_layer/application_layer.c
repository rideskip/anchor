#include "application_layer.h"

#include "types.h"

#define LOGGING_MODULE_NAME "SONAR"
#include "anchor/logging/logging.h"

#include <string.h>

typedef struct {
    bool is_active;
    bool pending_read_response;
    sonar_application_layer_header_t header;
    buffer_chain_entry_t header_buffer_chain;
    buffer_chain_entry_t data_buffer_chain;
} pending_request_info_t;

typedef struct {
    sonar_application_layer_init_t init;
    pending_request_info_t request;
} instance_impl_t;
_Static_assert(sizeof(sonar_application_layer_context_t) == sizeof(instance_impl_t), "Invalid context size");

static bool issue_request(instance_impl_t* inst, uint16_t attribute_id, uint16_t op, const uint8_t* data, uint32_t length) {
    if (inst->request.is_active) {
        LOG_ERROR("Application layer request already pending");
        return false;
    } else if (attribute_id & SONAR_APPLICATION_ATTRIBUTE_ID_OP_MASK) {
        LOG_ERROR("Invalid attribute ID: 0x%x", attribute_id);
        return false;
    }

    bool is_invalid_op;
    switch (op) {
    case SONAR_APPLICATION_ATTRIBUTE_ID_OP_READ:
    case SONAR_APPLICATION_ATTRIBUTE_ID_OP_WRITE:
        is_invalid_op = inst->init.is_server;
        break;
    case SONAR_APPLICATION_ATTRIBUTE_ID_OP_NOTIFY:
        is_invalid_op = !inst->init.is_server;
        break;
    default:
        // should never happen
        LOG_ERROR("Invalid application layer operation (%u)", op);
        return false;
    }
    if (is_invalid_op) {
        LOG_ERROR("Invalid application layer operation (is_server=%d, op=%u)", inst->init.is_server, op);
        return false;
    }

    inst->request.is_active = true;
    inst->request.header = (sonar_application_layer_header_t) {
        .attribute_id = attribute_id | op,
    };
    buffer_chain_set_data(&inst->request.data_buffer_chain, data, length);
    if (!inst->init.send_data_function(inst->init.send_data_handle, &inst->request.header_buffer_chain)) {
        inst->request.is_active = false;
        return false;
    }
    return true;
}

void sonar_application_layer_init(sonar_application_layer_handle_t handle, const sonar_application_layer_init_t* init) {
    instance_impl_t* inst = (instance_impl_t*)handle;
    *inst = (instance_impl_t){
        .init = *init,
    };
    buffer_chain_set_data(&inst->request.header_buffer_chain, (const uint8_t*)&inst->request.header, sizeof(inst->request.header));
    buffer_chain_push_back(&inst->request.header_buffer_chain, &inst->request.data_buffer_chain);
}

bool sonar_application_layer_read_request(sonar_application_layer_handle_t handle, uint16_t attribute_id) {
    instance_impl_t* inst = (instance_impl_t*)handle;
    return issue_request(inst, attribute_id, SONAR_APPLICATION_ATTRIBUTE_ID_OP_READ, NULL, 0);
}

bool sonar_application_layer_write_request(sonar_application_layer_handle_t handle, uint16_t attribute_id, const uint8_t* data, uint32_t length) {
    instance_impl_t* inst = (instance_impl_t*)handle;
    return issue_request(inst, attribute_id, SONAR_APPLICATION_ATTRIBUTE_ID_OP_WRITE, data, length);
}

bool sonar_application_layer_notify_request(sonar_application_layer_handle_t handle, uint16_t attribute_id, const uint8_t* data, uint32_t length) {
    instance_impl_t* inst = (instance_impl_t*)handle;
    return issue_request(inst, attribute_id, SONAR_APPLICATION_ATTRIBUTE_ID_OP_NOTIFY, data, length);
}

bool sonar_application_layer_handle_request(sonar_application_layer_handle_t handle, const uint8_t* data, uint32_t length) {
    instance_impl_t* inst = (instance_impl_t*)handle;
    if (length < sizeof(sonar_application_layer_header_t)) {
        LOG_ERROR("Invalid application layer packet: too short");
        return false;
    }

    // grab the header off the front of the data
    const sonar_application_layer_header_t* header = (const sonar_application_layer_header_t*)data;
    data += sizeof(*header);
    length -= sizeof(*header);

    // decode the header and call the corresponding operation handler
    const uint16_t op = header->attribute_id & SONAR_APPLICATION_ATTRIBUTE_ID_OP_MASK;
    const uint16_t attribute_id = header->attribute_id & SONAR_APPLICATION_ATTRIBUTE_ID_ATTRIBUTE_ID_MASK;
    switch (op) {
        case SONAR_APPLICATION_ATTRIBUTE_ID_OP_READ: {
            if (!inst->init.is_server) {
                LOG_ERROR("Invalid application layer packet: read request from server");
                return false;
            }
            if (length) {
                LOG_ERROR("Invalid application layer packet: read request with data (%"PRIu32")", length);
                return false;
            }
            inst->request.pending_read_response = true;
            const bool success = inst->init.attribute_read_handler(inst->init.attr_handler_handle, attribute_id);
            const bool set_response = !inst->request.pending_read_response;
            inst->request.pending_read_response = false;
            if (!success) {
                return false;
            } else if (!set_response) {
                // should never happen
                LOG_ERROR("No read response was set");
                return false;
            }
            return true;
        }
        case SONAR_APPLICATION_ATTRIBUTE_ID_OP_WRITE:
            if (!inst->init.is_server) {
                LOG_ERROR("Invalid application layer packet: write request from server");
                return false;
            }
            if (!inst->init.attribute_write_handler(inst->init.attr_handler_handle, attribute_id, data, length)) {
                return false;
            }
            inst->init.set_response_function(inst->init.send_data_handle, NULL, 0);
            return true;
        case SONAR_APPLICATION_ATTRIBUTE_ID_OP_NOTIFY:
            if (inst->init.is_server) {
                LOG_ERROR("Invalid application layer packet: notify request from client");
                return false;
            }
            if (!inst->init.attribute_notify_handler(inst->init.attr_handler_handle, attribute_id, data, length)) {
                return false;
            }
            inst->init.set_response_function(inst->init.send_data_handle, NULL, 0);
            return true;
        default:
            LOG_ERROR("Invalid application layer packet: invalid op (%u)", op);
            return false;
    }
}

void sonar_application_layer_handle_response(sonar_application_layer_handle_t handle, bool success, const uint8_t* data, uint32_t length) {
    instance_impl_t* inst = (instance_impl_t*)handle;
    if (!inst->request.is_active) {
        // should never happen
        LOG_ERROR("Unexpected response");
        return;
    }
    inst->request.is_active = false;
    const uint16_t attribute_id = inst->request.header.attribute_id & SONAR_APPLICATION_ATTRIBUTE_ID_ATTRIBUTE_ID_MASK;
    switch (inst->request.header.attribute_id & SONAR_APPLICATION_ATTRIBUTE_ID_OP_MASK) {
        case SONAR_APPLICATION_ATTRIBUTE_ID_OP_READ:
            inst->init.read_request_complete_handler(inst->init.request_complete_handle, attribute_id, success, data, length);
            break;
        case SONAR_APPLICATION_ATTRIBUTE_ID_OP_WRITE:
            inst->init.write_request_complete_handler(inst->init.request_complete_handle, attribute_id, success);
            break;
        case SONAR_APPLICATION_ATTRIBUTE_ID_OP_NOTIFY:
            inst->init.notify_request_complete_handler(inst->init.request_complete_handle, attribute_id, success);
            break;
        default:
            // should never happen
            LOG_ERROR("Invalid operation (0x%x)", inst->request.header.attribute_id);
            return;
    }
}

void sonar_application_layer_read_response(sonar_application_layer_handle_t handle, const uint8_t* data, uint32_t length) {
    instance_impl_t* inst = (instance_impl_t*)handle;
    if (!inst->request.pending_read_response) {
        LOG_ERROR("Unexpected read response");
        return;
    }
    inst->request.pending_read_response = false;
    inst->init.set_response_function(inst->init.send_data_handle, data, length);
}
