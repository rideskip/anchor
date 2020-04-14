#include "link_layer.h"

#include "receive.h"
#include "transmit.h"
#include "timeouts.h"

#define LOGGING_MODULE_NAME "SONAR"
#include "anchor/logging/logging.h"

#include <string.h>

typedef struct {
    bool is_active;
    uint8_t prev_sequence_num;
    uint64_t last_packet_time_ms;
} connection_info_t;

typedef struct {
    bool is_active;
    bool is_link_control;
    uint8_t sequence_num;
    uint64_t first_request_time_ms;
    uint64_t last_request_time_ms;
    const buffer_chain_entry_t* data;
} pending_request_info_t;

typedef struct {
    bool is_pending;
    bool is_active;
    bool is_link_control;
    uint8_t sequence_num;
    uint32_t length;
    const uint8_t* data;
} pending_response_info_t;

typedef struct {
    sonar_link_layer_init_t init;
    sonar_link_layer_errors_t errors;
    sonar_link_layer_receive_context_t receive_context;
    sonar_link_layer_transmit_context_t transmit_context;
    sonar_link_layer_receive_handle_t receive_handle;
    sonar_link_layer_transmit_handle_t transmit_handle;
    connection_info_t connection;
    buffer_chain_entry_t connection_data_buffer_chain;
    uint8_t connection_data;
    pending_request_info_t pending_request;
    pending_response_info_t pending_response;
} instance_impl_t;
_Static_assert(sizeof(sonar_link_layer_context_t) >= sizeof(instance_impl_t), "Invalid context size");

static void set_pending_request(instance_impl_t* inst, bool is_link_control, const buffer_chain_entry_t* data) {
    inst->pending_request.is_active = true;
    inst->pending_request.first_request_time_ms = inst->init.functions.get_system_time_ms();
    inst->pending_request.sequence_num++;
    inst->pending_request.is_link_control = is_link_control;
    inst->pending_request.data = data;
}

static void send_pending_request(instance_impl_t* inst) {
    inst->pending_request.last_request_time_ms = inst->init.functions.get_system_time_ms();
    sonar_link_layer_transmit_send_packet(inst->transmit_handle, false, inst->pending_request.is_link_control, inst->pending_request.sequence_num, inst->pending_request.data);
}

static void send_pending_response(instance_impl_t* inst) {
    buffer_chain_entry_t data = {0};
    buffer_chain_set_data(&data, inst->pending_response.data, inst->pending_response.length);
    sonar_link_layer_transmit_send_packet(inst->transmit_handle, true, inst->pending_response.is_link_control, inst->pending_response.sequence_num, &data);
}

static void disconnect(instance_impl_t* inst) {
    const bool had_pending_request = inst->pending_request.is_active;
    inst->pending_request.is_active = false;
    inst->connection.is_active = false;
    // need to clear the pending request and connected state before running the callbacks so that
    // the user doesn't try to issue a new request
    LOG_INFO("Disconnected");
    inst->init.handlers.connection_changed(inst->init.handlers.handler_handle, false);
    if (had_pending_request) {
        if (inst->pending_request.is_link_control) {
            LOG_INFO("Disconnected with link control request pending");
        } else {
            inst->init.handlers.request_complete(inst->init.handlers.handler_handle, false, NULL, 0);
        }
    }
}

static bool handle_link_control_packet(instance_impl_t* inst, bool is_response, uint8_t sequence_num, const uint8_t* data, uint32_t length) {
    if (inst->init.config.is_server == is_response) {
        // all link control responses go from the server to client only (and vice versa for requests)
        LOG_ERROR("Invalid packet: Wrong direction for link control packet");
        inst->errors.invalid_packet++;
        return false;
    }

    if (is_response) {
        if (length != 0) {
            // all responses should have 0 data bytes
            LOG_ERROR("Invalid packet: Link control packet with data");
            inst->errors.invalid_packet++;
            return false;
        }
        uint32_t request_length = 0;
        FOREACH_BUFFER_CHAIN_ENTRY(inst->pending_request.data, entry) {
            request_length += entry->length;
        }
        const bool did_connect = !inst->connection.is_active && inst->pending_request.is_link_control && request_length == 1;
        inst->pending_request.is_active = false;
        inst->connection.is_active = true;
        if (did_connect) {
            LOG_INFO("Connected");
            inst->init.handlers.connection_changed(inst->init.handlers.handler_handle, true);
        }
        return true;
    } else {
        // use the data length to figure out what type of request this is
        if (length == 0) {
            // connection maintenance request
            if (!inst->connection.is_active) {
                LOG_ERROR("Invalid packet: Connection maintenance request while not connected");
                inst->errors.unexpected_packet++;
                return false;
            }
        } else if (length == 1) {
            // connection request
            if (inst->connection.is_active) {
                // disconnect first since this is a new connection
                disconnect(inst);
            }
            LOG_INFO("Connected");
            // grab the data as our sequence number
            inst->pending_request.sequence_num = data[0] - 1;
            inst->connection.is_active = true;
            inst->init.handlers.connection_changed(inst->init.handlers.handler_handle, true);
        } else {
            LOG_ERROR("Invalid packet: Invalid link control data length (%"PRIu32")", length);
            inst->errors.invalid_packet++;
            return false;
        }

        // valid request, so send the response (always no data)
        inst->pending_response.is_active = true;
        inst->pending_response.sequence_num = sequence_num;
        inst->pending_response.is_link_control = true;
        inst->pending_response.data = NULL;
        inst->pending_response.length = 0;
        send_pending_response(inst);
        return true;
    }
}

static void receive_handler(void* handle, bool is_response, bool is_link_control, uint8_t sequence_num, const uint8_t* data, uint32_t length) {
    instance_impl_t* inst = handle;
    if (!is_link_control && !inst->connection.is_active) {
        LOG_ERROR("Invalid packet: Not connected");
            inst->errors.unexpected_packet++;
        return;
    } else if (is_response && !inst->pending_request.is_active) {
        LOG_ERROR("Invalid packet: Got response without any pending request");
        inst->errors.unexpected_packet++;
        return;
    } else if (is_response && sequence_num != inst->pending_request.sequence_num) {
        LOG_ERROR("Invalid packet: Response sequence number does not match request");
        inst->errors.invalid_sequence_number++;
        return;
    } else if (!is_link_control && !is_response && sequence_num == inst->connection.prev_sequence_num) {
        // this is a retry of the previous request (and not a link control request), so send the last response
        if (inst->pending_response.is_active) {
            send_pending_response(inst);
        } // else there was no response (request handler returned an error) so just drop this request
        return;
    } else if (!is_link_control && !is_response && (uint8_t)(sequence_num - 1) != inst->connection.prev_sequence_num) {
        LOG_ERROR("Invalid packet: Non-incrementing sequence number");
        inst->errors.invalid_sequence_number++;
        return;
    }

    if (is_link_control) {
        // handle_link_control_packet() is idempotent, so we can just call it every time and it'll also send the response
        if (!handle_link_control_packet(inst, is_response, sequence_num, data, length)) {
            return;
        }
        if (!is_response) {
            inst->connection.prev_sequence_num = sequence_num;
        }
    } else {
        if (is_response) {
            // mark the request as inactive first so the response handler can trigger another request
            inst->pending_request.is_active = false;
            inst->init.handlers.request_complete(inst->init.handlers.handler_handle, true, data, length);
        } else {
            // this was a valid packet as far as the link layer is concerned, so update our previous sequence number
            inst->connection.prev_sequence_num = sequence_num;
            inst->pending_response.is_active = false;
            inst->pending_response.is_pending = true;
            inst->pending_response.is_link_control = false;
            inst->pending_response.sequence_num = sequence_num;
            const bool success = inst->init.handlers.request(inst->init.handlers.handler_handle, data, length);
            const bool set_response = !inst->pending_response.is_pending;
            inst->pending_response.is_pending = false;
            if (!success) {
                // drop this packet
                return;
            } else if (!set_response) {
                LOG_ERROR("Request handler did not set a response");
                return;
            }

            // got a new request, so send the response
            send_pending_response(inst);
        }
    }

    // this was a valid packet, so update our last packet time
    inst->connection.last_packet_time_ms = inst->init.functions.get_system_time_ms();
}

void sonar_link_layer_init(sonar_link_layer_handle_t handle, const sonar_link_layer_init_t* init) {
    instance_impl_t* inst = (instance_impl_t*)handle;
    *inst = (instance_impl_t){
        .init = *init,
        .receive_handle = &inst->receive_context,
        .transmit_handle = &inst->transmit_context,
    };
    buffer_chain_set_data(&inst->connection_data_buffer_chain, (const uint8_t*)&inst->connection_data, sizeof(inst->connection_data));

    const sonar_link_layer_receive_init_t link_layer_receive_init = {
        .is_server = inst->init.config.is_server,
        .buffer = inst->init.buffers.receive,
        .buffer_size = inst->init.buffers.receive_size,
        .packet_handler = receive_handler,
        .handler_handle = inst,
    };
    sonar_link_layer_receive_init(inst->receive_handle, &link_layer_receive_init);

    const sonar_link_layer_transmit_init_t link_layer_transmit_init = {
        .is_server = inst->init.config.is_server,
        .write_byte_function = inst->init.functions.write_byte,
    };
    sonar_link_layer_transmit_init(inst->transmit_handle, &link_layer_transmit_init);
}

bool sonar_link_layer_is_connected(sonar_link_layer_handle_t handle) {
    instance_impl_t* inst = (instance_impl_t*)handle;
    return inst->connection.is_active;
}

void sonar_link_layer_handle_receive_data(sonar_link_layer_handle_t handle, const uint8_t* data, uint32_t length) {
    instance_impl_t* inst = (instance_impl_t*)handle;
    sonar_link_layer_receive_process_data(inst->receive_handle, data, length);
}

bool sonar_link_layer_send_request(sonar_link_layer_handle_t handle, const buffer_chain_entry_t* data) {
    instance_impl_t* inst = (instance_impl_t*)handle;
    if (!inst->connection.is_active) {
        LOG_ERROR("Not connected");
        return false;
    }
    if (inst->pending_request.is_active) {
        LOG_ERROR("ERROR: Request already pending");
        return false;
    }
    set_pending_request(inst, false, data);
    send_pending_request(inst);
    return true;
}

void sonar_link_layer_process(sonar_link_layer_handle_t handle) {
    instance_impl_t* inst = (instance_impl_t*)handle;
    const uint64_t time_ms = inst->init.functions.get_system_time_ms();
    const uint32_t ms_since_last_packet = time_ms - inst->connection.last_packet_time_ms;

    // check if the connection has timed out
    if (inst->connection.is_active && ms_since_last_packet >= CONNECTION_TIMEOUT_MS) {
        LOG_INFO("Connection timed out");
        disconnect(inst);
    }

    if (inst->pending_request.is_active) {
        // check if the pending request should be timed out or retried
        if (time_ms - inst->pending_request.first_request_time_ms >= REQUEST_TIMEOUT_MS) {
            // pending request has timed out
            inst->pending_request.is_active = false;
            if (inst->pending_request.is_link_control) {
                LOG_WARN("Link control request timed out");
            } else {
                LOG_WARN("Sonar request timed out");
                inst->init.handlers.request_complete(inst->init.handlers.handler_handle, false, NULL, 0);
            }
        } else if (time_ms - inst->pending_request.last_request_time_ms >= REQUEST_RETRY_INTERVAL_MS) {
            // send the request again
            send_pending_request(inst);
            inst->errors.retries++;
        }
    } else if (!inst->init.config.is_server) {
        // the bus is free so check if the client should send a link control request
        if (!inst->connection.is_active) {
            // try to connect (use a somewhat-random initial sequence number based on the time)
            inst->connection_data = time_ms & 0xff;
            inst->connection.prev_sequence_num = inst->connection_data - 1;
            set_pending_request(inst, true, &inst->connection_data_buffer_chain);
            send_pending_request(inst);
        } else if (ms_since_last_packet >= CONNECTION_MAINTENANCE_INTERVAL_MS) {
            // send a connection maintenance request
            set_pending_request(inst, true, NULL);
            send_pending_request(inst);
        }
    }
}

void sonar_link_layer_set_response(sonar_link_layer_handle_t handle, const uint8_t* data, uint32_t length) {
    instance_impl_t* inst = (instance_impl_t*)handle;
    if (!inst->pending_response.is_pending) {
        LOG_ERROR("Not pending a response");
        return;
    }
    inst->pending_response.is_pending = false;
    inst->pending_response.is_active = true;
    inst->pending_response.data = data;
    inst->pending_response.length = length;
}

void sonar_link_layer_get_and_clear_errors(sonar_link_layer_handle_t handle, sonar_link_layer_errors_t* errors, sonar_link_layer_receive_errors_t* receive_errors) {
    instance_impl_t* inst = (instance_impl_t*)handle;
    *errors = inst->errors;
    inst->errors = (sonar_link_layer_errors_t){0};
    sonar_link_layer_receive_get_and_clear_errors(inst->receive_handle, receive_errors);
}
