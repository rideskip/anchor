#pragma once

// Gets the corresponding attribute symbol for a protobuf message type
#define SONAR_PROTO_ATTR(PROTO_MSG_TYPE) PROTO_MSG_TYPE##_ATTR

// Calls SONAR_ATTR_DEF() to define an attribute with the specified protobuf message type
// The attribute symbol can be accessed via `SONAR_PROTO_ATTR(PROTO_MSG_TYPE)`
#define SONAR_PROTO_ATTR_DEF(PROTO_MSG_TYPE) \
    SONAR_PROTO_ATTR_DEF_WITH_NAME(PROTO_MSG_TYPE, SONAR_PROTO_ATTR(PROTO_MSG_TYPE))
#define SONAR_PROTO_ATTR_DEF_WITH_NAME(PROTO_MSG_TYPE, NAME) \
    _SONAR_PROTO_ATTR_DEF_IMPL(NAME, PROTO_MSG_TYPE, PROTO_MSG_TYPE##_ops)

// Helper macros for SONAR_PROTO_ATTR_*()
#define _SONAR_PROTO_ATTR_DEF_IMPL(NAME, PROTO_MSG_TYPE, OPS) \
    SONAR_ATTR_DEF(NAME, PROTO_MSG_TYPE##_msgid, PROTO_MSG_TYPE##_size, OPS)

// Calls SONAR_SERVER_ATTR_DEF() to define a server attribute with the specified protobuf
// message type. This macro also creates a wrapper around the read/write handler to take
// care of the encoding / decoding and change the prototypes to be:
//   static bool <PROTO_MSG_TYPE>_read_handler(PROTO_MSG_TYPE* msg);
//   static bool <PROTO_MSG_TYPE>_write_handler(const PROTO_MSG_TYPE* msg);
#define SONAR_SERVER_PROTO_ATTR_DEF(PROTO_MSG_TYPE, NAME) \
    _SONAR_SERVER_PROTO_ATTR_DEF_IMPL(NAME, PROTO_MSG_TYPE, PROTO_MSG_TYPE##_ops)
#define SONAR_SERVER_PROTO_ATTR_DEF_COPY(PROTO_MSG_TYPE, NAME) \
    _SONAR_SERVER_PROTO_ATTR_DEF_COPY_IMPL(NAME, PROTO_MSG_TYPE, PROTO_MSG_TYPE##_ops)
#define _SONAR_SERVER_PROTO_ATTR_DEF_COPY_IMPL(NAME, PROTO_MSG_TYPE, OPS) \
    _SONAR_SERVER_PROTO_ATTR_DEF_COPY_IMPL2(NAME, PROTO_MSG_TYPE, OPS)
#define _SONAR_SERVER_PROTO_ATTR_DEF_COPY_IMPL2(NAME, PROTO_MSG_TYPE, OPS) \
    SONAR_SERVER_ATTR_DEF_NO_PROTOTYPES(PROTO_MSG_TYPE##_ATTR, NAME, PROTO_MSG_TYPE##_msgid, PROTO_MSG_TYPE##_size, OPS)

// Helper macros for SONAR_SERVER_ATTR_*()
#define _SONAR_SERVER_PROTO_ATTR_DEF_IMPL(NAME, PROTO_MSG_TYPE, OPS) \
    _SONAR_SERVER_PROTO_ATTR_DEF_IMPL2(NAME, PROTO_MSG_TYPE, OPS)
#define _SONAR_SERVER_PROTO_ATTR_DEF_IMPL2(NAME, PROTO_MSG_TYPE, OPS) \
    _SONAR_SERVER_PROTO_ATTR_DEF_IMPL_##OPS(PROTO_MSG_TYPE) \
    SONAR_SERVER_ATTR_DEF(PROTO_MSG_TYPE##_ATTR, NAME, PROTO_MSG_TYPE##_msgid, PROTO_MSG_TYPE##_size, OPS)
#define _SONAR_SERVER_PROTO_ATTR_DEF_IMPL_R(PROTO_MSG_TYPE) \
    _SONAR_SERVER_PROTO_READ_HANDLER_DEF(PROTO_MSG_TYPE)
#define _SONAR_SERVER_PROTO_ATTR_DEF_IMPL_W(PROTO_MSG_TYPE) \
    _SONAR_SERVER_PROTO_WRITE_HANDLER_DEF(PROTO_MSG_TYPE)
#define _SONAR_SERVER_PROTO_ATTR_DEF_IMPL_N(PROTO_MSG_TYPE)
#define _SONAR_SERVER_PROTO_ATTR_DEF_IMPL_RW(PROTO_MSG_TYPE) \
    _SONAR_SERVER_PROTO_ATTR_DEF_IMPL_R(PROTO_MSG_TYPE) \
    _SONAR_SERVER_PROTO_ATTR_DEF_IMPL_W(PROTO_MSG_TYPE)
#define _SONAR_SERVER_PROTO_ATTR_DEF_IMPL_RN(PROTO_MSG_TYPE) \
    _SONAR_SERVER_PROTO_ATTR_DEF_IMPL_R(PROTO_MSG_TYPE)
#define _SONAR_SERVER_PROTO_ATTR_DEF_IMPL_WN(PROTO_MSG_TYPE) \
    _SONAR_SERVER_PROTO_ATTR_DEF_IMPL_W(PROTO_MSG_TYPE)
#define _SONAR_SERVER_PROTO_ATTR_DEF_IMPL_RWN(PROTO_MSG_TYPE) \
    _SONAR_SERVER_PROTO_ATTR_DEF_IMPL_RW(PROTO_MSG_TYPE)

#define _SONAR_SERVER_PROTO_READ_HANDLER_DEF(PROTO_MSG_TYPE) \
    static bool PROTO_MSG_TYPE##_read_handler(PROTO_MSG_TYPE* msg); \
    static uint32_t PROTO_MSG_TYPE##_ATTR_read_handler(void* response_data, uint32_t response_max_size) { \
        PROTO_MSG_TYPE msg = {}; \
        if (!PROTO_MSG_TYPE##_read_handler(&msg)) { \
            return 0; \
        }; \
        return SONAR_PROTO_ATTR_ENCODE(PROTO_MSG_TYPE, &msg, response_data, response_max_size); \
    }
#define _SONAR_SERVER_PROTO_WRITE_HANDLER_DEF(PROTO_MSG_TYPE) \
    static bool PROTO_MSG_TYPE##_write_handler(const PROTO_MSG_TYPE* msg); \
    static bool PROTO_MSG_TYPE##_ATTR_write_handler(const void* data, uint32_t length) { \
        PROTO_MSG_TYPE msg = {}; \
        if (!SONAR_PROTO_ATTR_DECODE(PROTO_MSG_TYPE, data, length, &msg)) { \
            return false; \
        } \
        return PROTO_MSG_TYPE##_write_handler(&msg); \
    }

#define SONAR_PROTO_ATTR_ENCODE(PROTO_MSG_TYPE, MSG_PTR, BUFFER, BUFFER_SIZE) ({ \
        uint32_t _result; \
        pb_ostream_t _stream = pb_ostream_from_buffer(BUFFER, BUFFER_SIZE); \
        if (pb_encode(&_stream, PROTO_MSG_TYPE##_fields, MSG_PTR)) { \
            _result = _stream.bytes_written; \
        } else { \
            LOG_ERROR("Failed to encode %s", #PROTO_MSG_TYPE); \
            _result = 0; \
        } \
        _result; \
    })

#define SONAR_PROTO_ATTR_DECODE(PROTO_MSG_TYPE, BUFFER, BUFFER_LEN, MSG_PTR) ({ \
        PROTO_MSG_TYPE _temp_msg; \
        pb_istream_t _stream = pb_istream_from_buffer(BUFFER, BUFFER_LEN); \
        bool _result = pb_decode(&_stream, PROTO_MSG_TYPE##_fields, &_temp_msg); \
        if (_result) { \
            *(MSG_PTR) = _temp_msg; \
        } else { \
            LOG_ERROR("Failed to decode %s", #PROTO_MSG_TYPE); \
        } \
        _result; \
    })
