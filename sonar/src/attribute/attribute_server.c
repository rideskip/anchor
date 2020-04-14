#include "server.h"

#include "../application_layer/types.h"
#include "control_helpers.h"

#define LOGGING_MODULE_NAME "SONAR"
#include "anchor/logging/logging.h"

#include <string.h>

#define GET_CONTEXT(IMPL_PTR) ((attribute_context_t*)(IMPL_PTR)->_private)

typedef struct {
    sonar_attribute_t next;
    bool is_registered;
} attribute_context_t;
_Static_assert(sizeof(attribute_context_t) == sizeof(((sonar_attribute_t)0)->_private), "Invalid size");

typedef struct {
    sonar_attribute_server_init_t init;
    sonar_attribute_t attr_list;
    CTRL_NUM_ATTRS_TYPE ctrl_num_attrs;
    CTRL_ATTR_OFFSET_TYPE ctrl_attr_offset;
    CTRL_ATTR_LIST_TYPE ctrl_attr_list;
} instance_impl_t;
_Static_assert(sizeof(instance_impl_t) == sizeof(sonar_attribute_server_context_t), "Invalid context size");

static sonar_attribute_t get_attr_by_id(instance_impl_t* inst, uint16_t attribute_id) {
    for (sonar_attribute_t attr = inst->attr_list; attr; attr = GET_CONTEXT(attr)->next) {
        if (attr->attribute_id == attribute_id) {
            return attr;
        }
    }
    return NULL;
}

static bool validate_attr_for_notify(instance_impl_t* inst, sonar_attribute_t attr) {
    if (!attr) {
        LOG_ERROR("Unknown attribute");
        return false;
    } else if (!(attr->ops & SONAR_ATTRIBUTE_OPS_N)) {
        LOG_ERROR("Notify not allowed for attribute (0x%x)", attr->attribute_id);
        return false;
    } else if (!GET_CONTEXT(attr)->is_registered) {
        LOG_ERROR("Attribute not registered");
        return false;
    }
    return true;
}

void sonar_attribute_server_init(sonar_attribute_server_handle_t handle, const sonar_attribute_server_init_t* init) {
    instance_impl_t* inst = (instance_impl_t*)handle;
    *inst = (instance_impl_t){
        .init = *init,
    };
}

void sonar_attribute_server_register(sonar_attribute_server_handle_t handle, sonar_attribute_t attr) {
    instance_impl_t* inst = (instance_impl_t*)handle;
    if (!attr) {
        // should never happen as these are all setup by SONAR macros
        LOG_ERROR("Invalid parameters");
        return;
    } else if (attr->attribute_id & SONAR_APPLICATION_ATTRIBUTE_ID_OP_MASK) {
        LOG_ERROR("Invalid attribute ID (0x%x)", attr->attribute_id);
        return;
    } else if (get_attr_by_id(inst, attr->attribute_id)) {
        LOG_ERROR("Attribute with this ID (0x%x) already registered", attr->attribute_id);
        return;
    }
    GET_CONTEXT(attr)->is_registered = true;
    if (inst->attr_list) {
        // add to the front of the list
        GET_CONTEXT(attr)->next = inst->attr_list;
        inst->attr_list = attr;
    } else {
        inst->attr_list = attr;
    }
    inst->ctrl_num_attrs++;
}

bool sonar_attribute_server_notify(sonar_attribute_server_handle_t handle, sonar_attribute_t attr, const uint8_t* data, uint32_t length) {
    instance_impl_t* inst = (instance_impl_t*)handle;
    if (!validate_attr_for_notify(inst, attr)) {
        return false;
    } else if (length > attr->max_size) {
        LOG_ERROR("Notify data is too big");
        return false;
    }
    memcpy(attr->request_buffer, data, length);
    return inst->init.send_notify_request_function(inst->init.handle, attr->attribute_id, attr->request_buffer, length);
}

bool sonar_attribute_server_notify_read_data(sonar_attribute_server_handle_t handle, sonar_attribute_t attr) {
    instance_impl_t* inst = (instance_impl_t*)handle;
    if (!validate_attr_for_notify(inst, attr)) {
        return false;
    } else if (!(attr->ops & SONAR_ATTRIBUTE_OPS_R)) {
        LOG_ERROR("Read request not supported");
        return false;
    }
    const uint32_t length = inst->init.read_handler(inst->init.handle, attr, attr->request_buffer, attr->max_size);
    if (length > attr->max_size) {
        LOG_ERROR("Notify data is too big");
        return false;
    }
    return inst->init.send_notify_request_function(inst->init.handle, attr->attribute_id, attr->request_buffer, length);
}

bool sonar_attribute_server_handle_read_request(sonar_attribute_server_handle_t handle, uint16_t attribute_id) {
    instance_impl_t* inst = (instance_impl_t*)handle;
    // handle control attributes explicitly inline here since they aren't registered
    if (attribute_id == CTRL_NUM_ATTRS_ID) {
        inst->init.read_response_handler(inst->init.handle, (const uint8_t*)&inst->ctrl_num_attrs, sizeof(inst->ctrl_num_attrs));
        return true;
    } else if (attribute_id == CTRL_ATTR_OFFSET_ID) {
        inst->init.read_response_handler(inst->init.handle, (const uint8_t*)&inst->ctrl_attr_offset, sizeof(inst->ctrl_attr_offset));
        return true;
    } else if (attribute_id == CTRL_ATTR_LIST_ID) {
        memset(inst->ctrl_attr_list, 0, sizeof(inst->ctrl_attr_list));
        uint16_t index = 0;
        uint16_t offset = inst->ctrl_attr_offset;
        for (sonar_attribute_t attr = inst->attr_list; attr && index < CTRL_ATTR_LIST_LENGTH; attr = GET_CONTEXT(attr)->next) {
            if (offset > 0) {
                // offset past this one
                offset--;
                continue;
            }
            inst->ctrl_attr_list[index++] = attr->attribute_id | attr->ops;
        }
        inst->init.read_response_handler(inst->init.handle, (const uint8_t*)inst->ctrl_attr_list, sizeof(inst->ctrl_attr_list));
        return true;
    }
    sonar_attribute_t attr = get_attr_by_id(inst, attribute_id);
    if (!attr) {
        LOG_ERROR("Got read request for unknown attribute (0x%x)", attribute_id);
        return false;
    } else if (!(attr->ops & SONAR_ATTRIBUTE_OPS_R)) {
        LOG_ERROR("Read request not supported for attribute (0x%x)", attribute_id);
        return false;
    }
    const uint32_t response_size = inst->init.read_handler(inst->init.handle, attr, attr->response_buffer, attr->max_size);
    inst->init.read_response_handler(inst->init.handle, attr->response_buffer, response_size);
    return true;
}

bool sonar_attribute_server_handle_write_request(sonar_attribute_server_handle_t handle, uint16_t attribute_id, const uint8_t* data, uint32_t length) {
    instance_impl_t* inst = (instance_impl_t*)handle;
    // handle control attributes explicitly inline here
    if (attribute_id == CTRL_ATTR_OFFSET_ID) {
        // CTRL_ATTR_OFFSET
        if (length != sizeof(CTRL_ATTR_OFFSET_TYPE)) {
            LOG_ERROR("Invalid request length (%"PRIu32") for CTRL_ATTR_OFFSET", length);
            return false;
        }
        memcpy(&inst->ctrl_attr_offset, data, length);
        return true;
    }
    sonar_attribute_t attr = get_attr_by_id(inst, attribute_id);
    if (!attr) {
        LOG_ERROR("Got write request for unknown attribute (0x%x)", attribute_id);
        return false;
    } else if (!(attr->ops & SONAR_ATTRIBUTE_OPS_W)) {
        LOG_ERROR("Write request not supported for attribute (0x%x)", attribute_id);
        return false;
    } else if (length > attr->max_size) {
        LOG_ERROR("Write request is too big (%"PRIu32") for attribute (0x%x)", length, attribute_id);
        return false;
    }
    return inst->init.write_handler(inst->init.handle, attr, data, length);
}

void sonar_attribute_server_handle_notify_response(sonar_attribute_server_handle_t handle, uint16_t attribute_id, bool success) {
    instance_impl_t* inst = (instance_impl_t*)handle;
    sonar_attribute_t attr = get_attr_by_id(inst, attribute_id);
    if (!attr || !(attr->ops & SONAR_ATTRIBUTE_OPS_N)) {
        // should never happen
        LOG_ERROR("Unexpected notify response");
        return;
    }
    inst->init.notify_complete_handler(inst->init.handle, success);
}
