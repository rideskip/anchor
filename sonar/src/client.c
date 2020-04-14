#include "anchor/sonar/client.h"

#include "link_layer/link_layer.h"
#include "application_layer/application_layer.h"
#include "attribute/client.h"

#define LOGGING_MODULE_NAME "SONAR"
#include "anchor/logging/logging.h"

typedef struct {
    sonar_client_init_t init;
    sonar_link_layer_context_t link_layer_context;
    sonar_application_layer_context_t application_layer_context;
    sonar_attribute_client_context_t attr_client_context;
    sonar_link_layer_handle_t link_layer_handle;
    sonar_application_layer_handle_t application_layer_handle;
    sonar_attribute_client_handle_t attr_client_handle;
} instance_impl_t;
_Static_assert(sizeof(instance_impl_t) == sizeof(((sonar_client_context_t*)0)->_private), "Invalid context size");

static void link_layer_connection_changed_handler(void* handle, bool connected) {
    instance_impl_t* inst = handle;
    return sonar_attribute_client_low_level_connection_changed(inst->attr_client_handle, connected);
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
    // the client should never got read requests
    LOG_ERROR("Got unexpected attribute read request (0x%x)", attribute_id);
    return false;
}

static bool application_layer_attribute_write_handler(void* handle, uint16_t attribute_id, const uint8_t* data, uint32_t length) {
    // the client should never got write requests
    LOG_ERROR("Got unexpected attribute write request (0x%x)", attribute_id);
    return false;
}

static bool application_layer_attribute_notify_handler(void* handle, uint16_t attribute_id, const uint8_t* data, uint32_t length) {
    return sonar_attribute_client_handle_notify_request(handle, attribute_id, data, length);
}

static void attribute_client_handle_read_response(void* handle, uint16_t attribute_id, bool success, const uint8_t* data, uint32_t length) {
    sonar_attribute_client_handle_read_response(handle, attribute_id, success, data, length);
}

static bool attribute_client_send_read_request_function(void* handle, uint16_t attribute_id) {
    instance_impl_t* inst = handle;
    return sonar_application_layer_read_request(inst->application_layer_handle, attribute_id);
}

static void attribute_client_handle_write_response(void* handle, uint16_t attribute_id, bool success) {
    sonar_attribute_client_handle_write_response(handle, attribute_id, success);
}

static bool attribute_client_send_write_request_function(void* handle, uint16_t attribute_id, const uint8_t* data, uint32_t length) {
    instance_impl_t* inst = handle;
    return sonar_application_layer_write_request(inst->application_layer_handle, attribute_id, data, length);
}

static void attribute_client_connection_changed_callback(void* handle, bool connected) {
    instance_impl_t* inst = handle;
    inst->init.connection_changed_callback(connected);
}

static void attribute_client_read_complete_handler(void* handle, bool success, const uint8_t* data, uint32_t length) {
    instance_impl_t* inst = handle;
    inst->init.attribute_read_complete_handler(success, data, length);
}

static void attribute_client_write_complete_handler(void* handle, bool success) {
    instance_impl_t* inst = handle;
    inst->init.attribute_write_complete_handler(success);
}

static bool attribute_client_notify_handler(void* handle, sonar_attribute_t attr, const uint8_t* data, uint32_t length) {
    instance_impl_t* inst = handle;
    return inst->init.attribute_notify_handler(attr, data, length);
}

void sonar_client_init(sonar_client_handle_t handle, const sonar_client_init_t* init) {
    instance_impl_t* inst = (instance_impl_t*)handle;
    *inst = (instance_impl_t){
        .init = *init,
        .link_layer_handle = &inst->link_layer_context,
        .application_layer_handle = &inst->application_layer_context,
        .attr_client_handle = &inst->attr_client_context,
    };
    const sonar_link_layer_init_t init_link_layer = {
        .config = {
            .is_server = false,
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
            .connection_changed = link_layer_connection_changed_handler,
            .request = link_layer_request_handler,
            .request_complete = link_layer_response_handler,
            .handler_handle = inst,
        },
    };
    sonar_link_layer_init(inst->link_layer_handle, &init_link_layer);

    const sonar_attribute_client_init_t init_attr_client = {
        .send_read_request_function = attribute_client_send_read_request_function,
        .send_write_request_function = attribute_client_send_write_request_function,
        .connection_changed_callback = attribute_client_connection_changed_callback,
        .read_complete_handler = attribute_client_read_complete_handler,
        .write_complete_handler = attribute_client_write_complete_handler,
        .notify_handler = attribute_client_notify_handler,
        .handle = inst,
    };
    sonar_attribute_client_init(inst->attr_client_handle, &init_attr_client);

    const sonar_application_layer_init_t init_application_layer = {
        .is_server = false,
        .send_data_function = application_layer_send_data_function,
        .set_response_function = application_layer_set_response_function,
        .send_data_handle = inst->link_layer_handle,
        .attribute_read_handler = application_layer_attribute_read_handler,
        .attribute_write_handler = application_layer_attribute_write_handler,
        .attribute_notify_handler = application_layer_attribute_notify_handler,
        .attr_handler_handle = inst->attr_client_handle,

        .read_request_complete_handler = attribute_client_handle_read_response,
        .write_request_complete_handler = attribute_client_handle_write_response,
        .request_complete_handle = inst->attr_client_handle,
    };
    sonar_application_layer_init(inst->application_layer_handle, &init_application_layer);
}

bool sonar_client_is_connected(sonar_client_handle_t handle) {
    instance_impl_t* inst = (instance_impl_t*)handle;
    return sonar_attribute_client_is_connected(inst->attr_client_handle);
}

void sonar_client_process(sonar_client_handle_t handle, const uint8_t* received_data, uint32_t received_data_length) {
    instance_impl_t* inst = (instance_impl_t*)handle;
    sonar_link_layer_handle_receive_data(inst->link_layer_handle, received_data, received_data_length);
    sonar_link_layer_process(inst->link_layer_handle);
}

void sonar_client_register(sonar_client_handle_t handle, sonar_attribute_t attr) {
    instance_impl_t* inst = ((instance_impl_t*)handle);
    sonar_attribute_client_register(inst->attr_client_handle, attr);
}

bool sonar_client_read(sonar_client_handle_t handle, sonar_attribute_t attr) {
    instance_impl_t* inst = (instance_impl_t*)handle;
    return sonar_attribute_client_read(inst->attr_client_handle, attr);
}

bool sonar_client_write(sonar_client_handle_t handle, sonar_attribute_t attr, const void* data, uint32_t length) {
    instance_impl_t* inst = (instance_impl_t*)handle;
    return sonar_attribute_client_write(inst->attr_client_handle, attr, data, length);
}

void sonar_client_get_and_clear_errors(sonar_client_handle_t handle, sonar_errors_t* errors) {
    instance_impl_t* inst = (instance_impl_t*)handle;
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
