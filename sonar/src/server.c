#include "anchor/sonar/server.h"

#include "link_layer/link_layer.h"
#include "application_layer/application_layer.h"
#include "attribute/server.h"

#define LOGGING_MODULE_NAME "SONAR"
#include "anchor/logging/logging.h"

#include <stddef.h>

#define GET_SERVER_IMPL(SERVER) ((instance_impl_t*)((SERVER)->_private))
#define GET_SERVER_ATTR_IMPL(SERVER) ((attr_instance_impl_t*)((SERVER)->_private))

typedef struct {
    sonar_server_init_t init;
    sonar_server_attribute_t attr_list;
    sonar_link_layer_context_t link_layer_context;
    sonar_application_layer_context_t application_layer_context;
    sonar_attribute_server_context_t attr_server_context;
    sonar_link_layer_handle_t link_layer_handle;
    sonar_application_layer_handle_t application_layer_handle;
    sonar_attribute_server_handle_t attr_server_handle;
} instance_impl_t;
_Static_assert(sizeof(instance_impl_t) == sizeof(((sonar_server_handle_t)0)->_private), "Invalid context size");

typedef struct {
    sonar_server_attribute_t next;
} attr_instance_impl_t;
_Static_assert(sizeof(attr_instance_impl_t) == sizeof(((sonar_server_attribute_t)0)->_private), "Invalid attribute context size");

static sonar_server_attribute_t get_server_attr(sonar_server_handle_t handle, sonar_attribute_t attr) {
    instance_impl_t* inst = GET_SERVER_IMPL(handle);
    sonar_server_attribute_t server_attr = inst->attr_list;
    while (server_attr) {
        if (server_attr->attr == attr) {
            return server_attr;
        }
        server_attr = GET_SERVER_ATTR_IMPL(server_attr)->next;
    }
    return NULL;
}

static void link_layer_connection_changed_callback(void* handle, bool connected) {
    instance_impl_t* inst = handle;
    inst->init.connection_changed_callback(handle, connected);
}

static bool link_layer_request_handler(void* handle, const uint8_t* data, uint32_t length) {
    instance_impl_t* inst = handle;
    return sonar_application_layer_handle_request(inst->application_layer_handle, data, length);
}

static void link_layer_response_handler(void* handle, bool success, const uint8_t* data, uint32_t length) {
    instance_impl_t* inst = handle;
    sonar_application_layer_handle_response(inst->application_layer_handle, success, data, length);
}

static bool application_layer_send_data_function(void* handle, const buffer_chain_entry_t* data) {
    return sonar_link_layer_send_request(handle, data);
}

static void application_layer_set_response_function(void* handle, const uint8_t* data, uint32_t length) {
    return sonar_link_layer_set_response(handle, data, length);
}

static bool application_layer_attribute_read_handler(void* handle, uint16_t attribute_id) {
    return sonar_attribute_server_handle_read_request(handle, attribute_id);
}

static bool application_layer_attribute_write_handler(void* handle, uint16_t attribute_id, const uint8_t* data, uint32_t length) {
    return sonar_attribute_server_handle_write_request(handle, attribute_id, data, length);
}

static bool application_layer_attribute_notify_handler(void* handle, uint16_t attribute_id, const uint8_t* data, uint32_t length) {
    // the server should never got notify requests
    LOG_ERROR("Got unexpected attribute notify request (0x%x)", attribute_id);
    return false;
}

static void attribute_server_handle_notify_response(void* handle, uint16_t attribute_id, bool success) {
    sonar_attribute_server_handle_notify_response(handle, attribute_id, success);
}

static bool attribute_server_send_notify_request_function(void* handle, uint16_t attribute_id, const uint8_t* data, uint32_t length) {
    instance_impl_t* inst = handle;
    return sonar_application_layer_notify_request(inst->application_layer_handle, attribute_id, data, length);
}

static void attribute_server_read_response_handler(void* handle, const uint8_t* data, uint32_t length) {
    instance_impl_t* inst = handle;
    sonar_application_layer_read_response(inst->application_layer_handle, data, length);
}

static uint32_t attribute_server_read_handler(void* handle, sonar_attribute_t attr, void* response_data, uint32_t response_max_size) {
    sonar_server_attribute_t server_attr = get_server_attr(handle, attr);
    if (!server_attr) {
        LOG_ERROR("Unknown attribute for read request");
        return 0;
    }
    return server_attr->read_handler(response_data, response_max_size);
}

static bool attribute_server_write_handler(void* handle, sonar_attribute_t attr, const uint8_t* data, uint32_t length) {
    sonar_server_attribute_t server_attr = get_server_attr(handle, attr);
    if (!server_attr) {
        LOG_ERROR("Unknown attribute for write request");
        return false;
    }
    return server_attr->write_handler(data, length);
}

static void attribute_server_notify_complete_handler(void* handle, bool success) {
    instance_impl_t* inst = handle;
    inst->init.attribute_notify_complete_handler(handle, success);
}

void sonar_server_init(sonar_server_handle_t handle, const sonar_server_init_t* init) {
    instance_impl_t* inst = GET_SERVER_IMPL(handle);
    *inst = (instance_impl_t){
        .init = *init,
        .link_layer_handle = &inst->link_layer_context,
        .application_layer_handle = &inst->application_layer_context,
        .attr_server_handle = &inst->attr_server_context,
    };
    const sonar_link_layer_init_t init_link_layer = {
        .config = {
            .is_server = true,
        },
        .buffers = {
            .receive = handle->receive_buffer,
            .receive_size = handle->receive_buffer_size,
        },
        .functions = {
            .get_system_time_ms = init->get_system_time_ms,
            .write_byte = init->write_byte,
        },
        .handlers = {
            .connection_changed = link_layer_connection_changed_callback,
            .request = link_layer_request_handler,
            .request_complete = link_layer_response_handler,
            .handler_handle = inst,
        },
    };
    sonar_link_layer_init(inst->link_layer_handle, &init_link_layer);

    const sonar_application_layer_init_t init_application_layer = {
        .is_server = true,
        .send_data_function = application_layer_send_data_function,
        .set_response_function = application_layer_set_response_function,
        .send_data_handle = inst->link_layer_handle,
        .attribute_read_handler = application_layer_attribute_read_handler,
        .attribute_write_handler = application_layer_attribute_write_handler,
        .attribute_notify_handler = application_layer_attribute_notify_handler,
        .attr_handler_handle = inst->attr_server_handle,

        .notify_request_complete_handler = attribute_server_handle_notify_response,
        .request_complete_handle = inst->attr_server_handle,
    };
    sonar_application_layer_init(inst->application_layer_handle, &init_application_layer);

    const sonar_attribute_server_init_t init_attr_server = {
        .send_notify_request_function = attribute_server_send_notify_request_function,
        .read_response_handler = attribute_server_read_response_handler,
        .read_handler = attribute_server_read_handler,
        .write_handler = attribute_server_write_handler,
        .notify_complete_handler = attribute_server_notify_complete_handler,
        .handle = inst,
    };
    sonar_attribute_server_init(inst->attr_server_handle, &init_attr_server);
}

bool sonar_server_is_connected(sonar_server_handle_t handle) {
    instance_impl_t* inst = GET_SERVER_IMPL(handle);
    return sonar_link_layer_is_connected(inst->link_layer_handle);
}

void sonar_server_process(sonar_server_handle_t handle, const uint8_t* received_data, uint32_t received_data_length) {
    instance_impl_t* inst = GET_SERVER_IMPL(handle);
    sonar_link_layer_handle_receive_data(inst->link_layer_handle, received_data, received_data_length);
    sonar_link_layer_process(inst->link_layer_handle);
}

void sonar_server_register(sonar_server_handle_t handle, sonar_server_attribute_t attr) {
    instance_impl_t* inst = GET_SERVER_IMPL(handle);
    if (inst->attr_list) {
        GET_SERVER_ATTR_IMPL(attr)->next = inst->attr_list;
    }
    inst->attr_list = attr;
    sonar_attribute_server_register(inst->attr_server_handle, attr->attr);
}

bool sonar_server_notify(sonar_server_handle_t handle, sonar_server_attribute_t attr, const void* data, uint32_t length) {
    instance_impl_t* inst = GET_SERVER_IMPL(handle);
    return sonar_attribute_server_notify(inst->attr_server_handle, attr->attr, data, length);
}

bool sonar_server_notify_read_data(sonar_server_handle_t handle, sonar_server_attribute_t attr) {
    instance_impl_t* inst = GET_SERVER_IMPL(handle);
    return sonar_attribute_server_notify_read_data(inst->attr_server_handle, attr->attr);
}

void sonar_server_get_and_clear_errors(sonar_server_handle_t handle, sonar_errors_t* errors) {
    instance_impl_t* inst = GET_SERVER_IMPL(handle);
    sonar_link_layer_errors_t link_layer_errors;
    sonar_link_layer_receive_errors_t link_layer_receive_errors;
    sonar_link_layer_get_and_clear_errors(inst->link_layer_handle, &link_layer_errors, &link_layer_receive_errors);
    *errors = (sonar_errors_t){
        .link_layer_receive = {
            .invalid_header = link_layer_receive_errors.invalid_header,
            .invalid_crc = link_layer_receive_errors.invalid_crc,
            .buffer_overflow = link_layer_receive_errors.buffer_overflow,
            .invalid_escape_sequence = link_layer_receive_errors.invalid_escape_sequence,
        },
        .link_layer = {
            .invalid_packet = link_layer_errors.invalid_packet,
            .unexpected_packet = link_layer_errors.unexpected_packet,
            .invalid_sequence_number = link_layer_errors.invalid_sequence_number,
            .retries = link_layer_errors.retries,
        },
    };
}
