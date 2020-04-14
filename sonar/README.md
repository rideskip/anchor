# SONAR

This library contains an implementation of the SONAR communication protocol as
described by the [Protocol Specification](ProtocolSpecification.md). The
library is written such that it can be instantiated multiple times (as either
a client, server, or both), with no internal static state. This is achieved by
providing macros for the application code to define any static memory used to
store client, server, or attribute state.

## Attributes

Attributes represent either configuration or state data on the server and are
the unit of data which is sent (as a read / write / notify request) between the
client and server.

It's recommended (but not strictly required) to use
(protobuf)[https://developers.google.com/protocol-buffers] for defining
attributes. The SONAR library includes a plugin which makes it easy to use
such attributes with SONAR. Note that SONAR attributes need to have a known
max size, so it's suggested to also use the appropriate properties of a
protobuf implementation such as [nanopb](https://github.com/nanopb/nanopb).

### Protobuf Extensions

When defining a protobuf message for an attribute, the extensions defined in
(sonar_extensions.proto)[protobuf/sonar_extensions.proto] should be added. For
example, to define a `DeviceInfo` attribute with attribute ID `0x264` which
supports read and notify requests:
```protobuf
message DeviceInfo {
    option (nanopb_msgopt).msgid = 0x264;
    option (sonar_msgopt).attr_ops = ATTR_OPS_R;

    enum HardwareRevision {
        UNKNOWN = 0;
        EVT = 1;
        DVT = 2;
        PVT = 3;
    }

    HardwareRevision hardware_revision = 1;
    string serial = 2 [(nanopb).max_size = 25];
}
```

### Protoc Plugin

SONAR's protoc plugin is used to extend the generated protobuf code (currently
supports C, Java, and Python). The recommended way to leverage this for C code
is via the [sonar_nanopb.mk](protobuf/sonar_nanopb.mk) GNU make file which
contains rules for using nanopb to turn .proto files into .pb.c/.pb.h files,
and will include the SONAR protoc plugin.

## Server

The server is responsible for exposing a set of attributes to clients, handling
read / write requests, and sending notify requests to the client.

### Initialization and Processing

A SONAR server instance should first be defined using the
`SONAR_SERVER_DEF()` macro. This instance is then initialized by calling
`sonar_server_init()`. This function takes a few function pointers which are
used by the SONAR server implementation:
* `write_byte` - sends a byte of data over the physical layer to the client
* `get_system_time_ms` - gets the current system time in ms to manage various
retries and timeouts
* `connection_changed_callback` - called when a client connects or disconnects
* `attribute_notify_complete_handler` called when a notify request completes

The `sonar_server_process()` function should be called regularly to allow the
library to process any pending requests and handle timeouts. This function
should be passed in any data which was received since the last time it was
called, but should still be called even if no new data is available to handle
any applicable connection and notify timeouts. The timeouts (defined in
[timeouts.h](src/link_layer/timeouts.h)) create a lower bound of 100ms for the
minimum interval which this should be called at.

### Attributes

A server attribute whose data is defined via protobuf can be defined using the
`SONAR_SERVER_PROTO_ATTR_DEF(PROTO_MSG_TYPE, NAME)` macro. This macro defines
read and write handlers for the attributes (if those operations are supported)
which handle the encoding/decoding of the attribute using nanopb. The macro
defines the following prototypes which the application code must define (as
applicable based on the supported requests) which it then calls:
```c
static bool <PROTO_MSG_TYPE>_read_handler(<PROTO_MSG_TYPE>* msg);
static bool <PROTO_MSG_TYPE>_write_handler(const <PROTO_MSG_TYPE>* msg);
```

Attributes are then registered with a SONAR server using the
`sonar_server_register()` function. A notify can be sent to the client by
calling `sonar_server_notify()`. Once the request is complete (either
succeeds or fails), the `attribute_notify_complete_handler` which was
previously specified will be called.

## Client

The client connects to a server, issues read / write requests against its
attributes, and handles notify requests from the server.

### Initialization and Processing

A SONAR client instance should first be defined using the
`SONAR_CLIENT_DEF()` macro. This instance is then initialized by calling
`sonar_client_init()`. This function takes a few function pointers which are
used by the SONAR client implementation:
* `write_byte` - sends a byte of data over the physical layer to the server
* `get_system_time_ms` - gets the current system time in ms to manage various
retries and timeouts
* `connection_changed_callback` - called when a client connects or disconnects
* `attribute_read_complete_handler` - called when a read request completes
* `attribute_write_complete_handler` - called when a write request completes
* `attribute_notify_handler` handles a notify requests

The `sonar_client_process()` function should be called regularly to allow the
library to process any pending requests and handle timeouts. This function
should be passed in any data which was received since the last time it was
called, but should still be called even if no new data is available to handle
any applicable connection and notify timeouts. The timeouts (defined in
[timeouts.h](src/link_layer/timeouts.h)) create a lower bound of 100ms for the
minimum interval which this should be called at.

### Attributes

Attributes are registered with a SONAR client using `sonar_client_register()`.
Notify requests are then handled via the `attribute_notify_handler` function
which was previously specified.

A read / write can be sent to the server by calling `sonar_client_read()` and
`sonar_client_write()` functions respectively. If the attribute is not
supported by the server, these functions will return `false`. Once the request
is complete (either succeeds or fails), the appropriate
`attribute_*_complete_handler` which was previously specified will be called.

## Tests

The unit tests can be run by running `make` within the `tests` directory.
These tests are built using
[googletest](https://github.com/google/googletest) and depend on the
`gtest` library being available on the system.

## Example

Server:
```c
#include "sonar_device_info.pb.h"

SONAR_SERVER_DEF(m_server, 1024);
SONAR_PROTO_ATTR_DEF(Sonar_DeviceInfo, SONAR_DEVICE_INFO);

static void write_byte(uint8_t byte) {
    serial_write(&byte, sizeof(byte));
}

static uint64_t get_system_time_ms(void) {
    return GET_SYS_TIME();
}

static void connection_changed_callback(sonar_server_handle_t handle, bool connected) {
    LOG_INFO("Connection changed to %d", connected);
}

static bool Sonar_DeviceInfo_read_handler(Sonar_DeviceInfo* msg) {
    msg->hardware_revision = Sonar_DeviceInfo_HardwareRevision_PVT;
    strncpy(msg->serial, "123456789", sizeof(msg->serial));
    return true;
}

static void attribute_notify_complete_handler(sonar_server_handle_t handle, bool success) {
    LOG_INFO("Notify complete (success=%d)", success);
}

void sonar_server_task(void) {
    const sonar_server_init_t init_server = {
        .write_byte = write_byte,
        .get_system_time_ms = get_system_time_ms,
        .connection_changed_callback = connection_changed_callback,
        .attribute_notify_complete_handler = attribute_notify_complete_handler,
    };
    sonar_server_init(m_server, &init_server);
    sonar_server_register(m_server, SONAR_DEVICE_INFO);

    while (1) {
        uint8_t rx_buffer[64];
        uint32_t rx_len = serial_read(rx_buffer, sizeof(rx_buffer));
        sonar_server_process(m_server, rx_buffer, rx_len);
        // ...
    }
}
```
Client:

```c
SONAR_CLIENT_DEF(m_client, 1024);
SONAR_PROTO_ATTR_DEF(Sonar_DeviceInfo);

static uint32_t m_data_value = 0x42424242;

static void write_byte(uint8_t byte) {
    serial_write(&byte, sizeof(byte));
}

static uint64_t get_system_time_ms(void) {
    return GET_SYS_TIME();
}

static void connection_changed_callback(sonar_server_handle_t handle, bool connected) {
    LOG_INFO("Connection changed to %d", connected);
}

static void attribute_read_complete_handler(bool success, const void* data, uint32_t length) {
    Sonar_DeviceInfo msg;
    if (!SONAR_PROTO_ATTR_DECODE(Sonar_DeviceInfo, data, length, &msg)) {
        return;
    }
    LOG_INFO("Read complete (serial=%s)", msg.serial);
}

static void attribute_write_complete_handler(bool success) {
    LOG_INFO("Write complete");
}

static bool attribute_notify_handler(sonar_attribute_t attr, const void* data, uint32_t length) {
    // ...
    return false;
}

void sonar_client_task(void) {
    const sonar_client_init_t init_client = {
        .write_byte = write_byte,
        .get_system_time_ms = get_system_time_ms,
        .connection_changed_callback = connection_changed_callback,
        .attribute_read_complete_handler = attribute_read_complete_handler,
        .attribute_write_complete_handler = attribute_write_complete_handler,
        .attribute_notify_handler = attribute_notify_handler,
    };
    sonar_client_init(m_client, &init_client);
    sonar_client_register(m_client, SONAR_PROTO_ATTR(Sonar_DeviceInfo));

    while (1) {
        uint8_t rx_buffer[64];
        uint32_t rx_len = serial_read(rx_buffer, sizeof(rx_buffer));
        sonar_client_process(m_client, rx_buffer, rx_len);
        if (/* should write to TEST_ATTR */) {
            sonar_client_read(m_client, SONAR_PROTO_ATTR(Sonar_DeviceInfo));
        }
        // ...
    }
}
```
