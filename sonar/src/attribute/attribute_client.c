#include "client.h"

#include "../application_layer/types.h"
#include "control_helpers.h"

#define LOGGING_MODULE_NAME "SONAR"
#include "anchor/logging/logging.h"

#include <stdbool.h>
#include <string.h>

#define GET_CONTEXT(IMPL_PTR) ((attribute_context_t*)&(IMPL_PTR)->_private)

typedef struct {
    sonar_attribute_def_t* next;
    bool is_available;
    bool is_registered;
} attribute_context_t;
_Static_assert(sizeof(attribute_context_t) == sizeof(((sonar_attribute_def_t*)0)->_private), "Invalid size");

typedef struct {
    sonar_attribute_client_init_t init;
    sonar_attribute_def_t* def_list;
    uint16_t num_attrs;
    uint16_t attr_offset;
    bool is_connected;
} instance_impl_t;
_Static_assert(sizeof(instance_impl_t) == sizeof(sonar_attribute_client_context_t), "Invalid context size");

static sonar_attribute_def_t* get_def_by_id(instance_impl_t* inst, uint16_t attribute_id) {
    for (sonar_attribute_def_t* def = inst->def_list; def; def = GET_CONTEXT(def)->next) {
        if (def->attribute_id == attribute_id) {
            return def;
        }
    }
    return NULL;
}

static bool send_attribute_read(instance_impl_t* inst, uint16_t attribute_id) {
    return inst->init.send_read_request_function(inst->init.handle, attribute_id);
}

static bool send_attribute_write(instance_impl_t* inst, uint16_t attribute_id, const uint8_t* data, uint32_t length) {
    return inst->init.send_write_request_function(inst->init.handle, attribute_id, data, length);
}

static void disconnect(instance_impl_t* inst) {
    inst->is_connected = false;
    inst->init.connection_changed_callback(inst->init.handle, false);
}

static void num_attrs_read_complete(instance_impl_t* inst, bool success, const uint8_t* data, uint32_t length) {
    if (!success) {
        // failed to read, so disconnect
        LOG_ERROR("Failed to read num_attrs");
        disconnect(inst);
        return;
    }

    memcpy(&inst->num_attrs, data, sizeof(inst->num_attrs));

    // set the initial offset to 0 and write it to the server
    inst->attr_offset = 0;
    if (!send_attribute_write(inst, 0x102, (const uint8_t*)&inst->attr_offset, sizeof(inst->attr_offset))) {
        // should never happen
        LOG_ERROR("Failed to write CTRL_ATTR_OFFSET");
    }
}

static void attr_list_read_complete(instance_impl_t* inst, bool success, const uint8_t* data, uint32_t length) {
    if (!success) {
        // failed to read, so disconnect
        LOG_ERROR("Failed to read attr_list");
        disconnect(inst);
        return;
    }

    // mark all the received attr ids as available
    uint16_t num_attr_ids = inst->num_attrs - inst->attr_offset;
    const bool has_more = num_attr_ids > CTRL_ATTR_LIST_LENGTH;
    if (has_more) {
        num_attr_ids = CTRL_ATTR_LIST_LENGTH;
    }
    const uint16_t* attr_list = (const uint16_t*)data;
    for (uint16_t i = 0; i < num_attr_ids; i++) {
        const uint16_t attribute_id = attr_list[i];
        const sonar_attribute_def_t* def = get_def_by_id(inst, attribute_id & SONAR_APPLICATION_ATTRIBUTE_ID_ATTRIBUTE_ID_MASK);
        if (!def) {
            // not supported locally, so ignore
            continue;
        } else if (attribute_id & CTRL_ATTR_LIST_OP_BIT_RESERVED) {
            LOG_ERROR("Invalid attribute ops for attribute (0x%x)", attribute_id);
            continue;
        } else if ((uint16_t)def->ops != (attribute_id & 0xf000)) {
            // ops mismatch between the client and the server
            continue;
        }
        GET_CONTEXT(def)->is_available = true;
    }

    if (has_more) {
        // advance the offset to read the next chunk
        inst->attr_offset += CTRL_ATTR_LIST_LENGTH;
        if (!send_attribute_write(inst, CTRL_ATTR_OFFSET_ID, (const uint8_t*)&inst->attr_offset, sizeof(inst->attr_offset))) {
            // should never happen
            LOG_ERROR("Failed to write CTRL_ATTR_OFFSET");
            return;
        }
    } else {
        // we're now connected
        LOG_INFO("Connected");
        inst->is_connected = true;
        inst->init.connection_changed_callback(inst->init.handle, true);
    }
}

static void attr_offset_write_complete(instance_impl_t* inst, bool success) {
    if (!success) {
        // failed to write, so disconnect
        LOG_ERROR("Failed to write attr_offset");
        disconnect(inst);
        return;
    }

    // read the attr ids at this offset
    if (!send_attribute_read(inst, CTRL_ATTR_LIST_ID)) {
        // should never happen
        LOG_ERROR("Failed to read CTRL_ATTR_LIST");
        return;
    }
}

void sonar_attribute_client_init(sonar_attribute_client_handle_t handle, const sonar_attribute_client_init_t* init) {
    instance_impl_t* inst = (instance_impl_t*)handle;
    *inst = (instance_impl_t){
        .init = *init,
    };
}

void sonar_attribute_client_register(sonar_attribute_client_handle_t handle, sonar_attribute_t attr) {
    sonar_attribute_def_t* def = attr;
    instance_impl_t* inst = (instance_impl_t*)handle;
    if (!def) {
        // should never happen as these are all setup by SONAR macros
        LOG_ERROR("Invalid parameters");
        return;
    } else if (def->attribute_id & SONAR_APPLICATION_ATTRIBUTE_ID_OP_MASK) {
        LOG_ERROR("Invalid attribute ID (0x%x)", def->attribute_id);
        return;
    } else if (get_def_by_id(inst, def->attribute_id)) {
        LOG_ERROR("Attribute with this ID (0x%x) already registered", def->attribute_id);
        return;
    } else if (inst->is_connected) {
        // should never happen
        LOG_ERROR("Must register all attributes before a connection is established");
        return;
    }
    GET_CONTEXT(def)->is_registered = true;
    if (inst->def_list) {
        // add to the front of the list
        GET_CONTEXT(def)->next = inst->def_list;
        inst->def_list = def;
    } else {
        inst->def_list = def;
    }
}

void sonar_attribute_client_low_level_connection_changed(sonar_attribute_client_handle_t handle, bool is_connected) {
    instance_impl_t* inst = (instance_impl_t*)handle;
    if (is_connected) {
        // kick off server attribute enumeration by reading the number of attributes
        if (!send_attribute_read(inst, CTRL_NUM_ATTRS_ID)) {
            // should never happen
            LOG_ERROR("Failed to read CTRL_NUM_ATTRS");
            return;
        }
    } else {
        // mark all attribute as unavailable
        LOG_INFO("Disconnected");
        for (const sonar_attribute_def_t* def = inst->def_list; def; def = GET_CONTEXT(def)->next) {
            GET_CONTEXT(def)->is_available = false;
        }
        inst->is_connected = false;
        inst->init.connection_changed_callback(inst->init.handle, false);
    }
}

bool sonar_attribute_client_is_connected(sonar_attribute_client_handle_t handle) {
    instance_impl_t* inst = (instance_impl_t*)handle;
    return inst->is_connected;
}

bool sonar_attribute_client_read(sonar_attribute_client_handle_t handle, sonar_attribute_t attr) {
    instance_impl_t* inst = (instance_impl_t*)handle;
    const sonar_attribute_def_t* def = attr;
    if (!def) {
        LOG_ERROR("Unknown attribute");
        return false;
    } else if (!(def->ops & SONAR_ATTRIBUTE_OPS_R)) {
        LOG_ERROR("Read not allowed for attribute (0x%x)", def->attribute_id);
        return false;
    } else if (!GET_CONTEXT(def)->is_registered) {
        LOG_ERROR("Attribute not registered");
        return false;
    } else if (!GET_CONTEXT(def)->is_available) {
        LOG_ERROR("Attribute not available");
        return false;
    }
    return send_attribute_read(inst, def->attribute_id);
}

bool sonar_attribute_client_write(sonar_attribute_client_handle_t handle, sonar_attribute_t attr, const uint8_t* data, uint32_t length) {
    instance_impl_t* inst = (instance_impl_t*)handle;
    const sonar_attribute_def_t* def = attr;
    if (!def) {
        LOG_ERROR("Unknown attribute");
        return false;
    } else if (!(def->ops & SONAR_ATTRIBUTE_OPS_W)) {
        LOG_ERROR("Write not allowed for attribute (0x%x)", def->attribute_id);
        return false;
    } else if (!GET_CONTEXT(def)->is_registered) {
        LOG_ERROR("Attribute not registered");
        return false;
    } else if (!GET_CONTEXT(def)->is_available) {
        LOG_ERROR("Attribute not available");
        return false;
    } else if (length > def->max_size) {
        LOG_ERROR("Write data is too big");
        return false;
    }
    memcpy(def->request_buffer, data, length);
    return send_attribute_write(inst, def->attribute_id, def->request_buffer, length);
}

void sonar_attribute_client_handle_read_response(sonar_attribute_client_handle_t handle, uint16_t attribute_id, bool success, const uint8_t* data, uint32_t length) {
    instance_impl_t* inst = (instance_impl_t*)handle;
    // handle control attributes explicitly inline here since they aren't registered
    if (attribute_id == CTRL_NUM_ATTRS_ID) {
        num_attrs_read_complete(inst, success, data, length);
        return;
    } else if (attribute_id == CTRL_ATTR_LIST_ID) {
        attr_list_read_complete(inst, success, data, length);
        return;
    }
    const sonar_attribute_def_t* def = get_def_by_id(inst, attribute_id);
    if (!def || !(def->ops & SONAR_ATTRIBUTE_OPS_R)) {
        // should never happen
        LOG_ERROR("Unexpected read response");
        return;
    } else if (success && length > def->max_size) {
        LOG_ERROR("Read response is too big (%"PRIu32") for attribute (0x%x)", length, attribute_id);
        return;
    } else if (!GET_CONTEXT(def)->is_available) {
        // this could happen if we've recently disconnected
        LOG_ERROR("Unexpected read response for unavailable attribute (0x%x)", attribute_id);
        return;
    }
    inst->init.read_complete_handler(inst->init.handle, success, data, length);
}

void sonar_attribute_client_handle_write_response(sonar_attribute_client_handle_t handle, uint16_t attribute_id, bool success) {
    instance_impl_t* inst = (instance_impl_t*)handle;
    // handle control attributes explicitly inline here since they aren't registered
    if (attribute_id == CTRL_ATTR_OFFSET_ID) {
        attr_offset_write_complete(inst, success);
        return;
    }
    const sonar_attribute_def_t* def = get_def_by_id(inst, attribute_id);
    if (!def || !(def->ops & SONAR_ATTRIBUTE_OPS_W)) {
        // should never happen
        LOG_ERROR("Unexpected write response");
        return;
    } else if (!GET_CONTEXT(def)->is_available) {
        // this could happen if we've recently disconnected
        LOG_ERROR("Unexpected write response for unavailable attribute");
        return;
    }
    inst->init.write_complete_handler(inst->init.handle, success);
}

bool sonar_attribute_client_handle_notify_request(sonar_attribute_client_handle_t handle, uint16_t attribute_id, const uint8_t* data, uint32_t length) {
    instance_impl_t* inst = (instance_impl_t*)handle;
    sonar_attribute_def_t* def = get_def_by_id(inst, attribute_id);
    if (!def) {
        LOG_ERROR("Got notify request for unknown attribute (0x%x)", attribute_id);
        return false;
    } else if (!(def->ops & SONAR_ATTRIBUTE_OPS_N)) {
        LOG_ERROR("Notify request not supported for attribute (0x%x)", attribute_id);
        return false;
    } else if (length > def->max_size) {
        LOG_ERROR("Notify request is too big (%"PRIu32") for attribute (0x%x)", length, attribute_id);
        return false;
    } else if (!GET_CONTEXT(def)->is_available) {
        LOG_ERROR("Notify request for an attribute which is not available");
        return false;
    }
    return inst->init.notify_handler(inst->init.handle, def, data, length);
}
