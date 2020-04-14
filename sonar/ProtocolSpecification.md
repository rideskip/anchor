# SONAR Specification

# Overview

SONAR is point-to-point, client-server data communication protocol which allows two endpoints to reliably communicate over an arbitrary physical layer. SONAR draws inspiration from HDLC / TCP for its link layer, and USB / BLE for its application layer (among others).

SONAR is designed with the following goals in mind:

- Usable over a lossy physical layer
- As simple as possible to satisfy typical use-cases, but flexible enough to adapt to future requirements
- Low, fixed (at compile time) memory requirements on both endpoints
- Easily implemented on a very resource-constrained MCU (minimal protocol overhead)

On the flip side, SONAR was NOT designed for the following use-cases:

- Any case where secure communication or authentication is required
- Message routing over a 1-to-Many or many-to-Many communication bus
- High bandwidth or very low latency applications
- Symmetrical and bidirectional querying of data

# Terminology

Below is a list of terms used throughout this spec and their definitions.

- **Server** - The server is the endpoint which exposes attributes and controls which the client can interact with. It is responsible for responding to commands and requests from the client. It may also send unrequested data to the client in the form of notifications.
- **Client** - The client is the endpoint which sends commands and requests to the server.
- **Attribute** - An attribute is a data value which is exchanged between the server and the client. It is identified by a unique ID, supports a set of predefined operations, and has a user-defined format.

# Link Layer

The link layer is responsible for getting data packets between the client and server in a reliable way. It requires that the physical layer can operate with an 8-bit granularity and that bits / bytes are delivered in order, but does not make any assumptions around data integrity.

## Framing / Encoding

The SONAR link layer does not rely on the physical layer to mark the beginning and end of frames. Therefore, it introduces its own framing mechanism. This framing mechanism is heavily inspired by [HDLC's framing mechanism](https://en.wikipedia.org/wiki/High-Level_Data_Link_Control). Frames are delimited with a flag byte (0x7e). A control byte (0x7d) is used to escape flag bytes which exist within the data. If either a flag byte (0x7e) or control byte (0x7d) exists within the data, it is escaped by first inserting a control byte (0x7d) and then XOR’ing the data byte with 0x20. For example, a data sequence of “0x11 0x7d 0x22 0x7e 0x33” is encoded by the link layer as “0x11 **0x7d** 0x5d 0x22 **0x7d** 0x5e 0x33” (before the framing bytes are added). This framing and byte stuffing scheme is then reversed on the received before the data packet gets passed up to the higher layers. Any decoding error on the receiver results in the frame being silently discarded. For robustness, the linker layer sends a flag byte at both the start and end of each frame, and the receiver should silently discard the potential zero-length data packets which exists between two actual data frames.

## Packet Format

The link layer defines the following packet format.

| **Flags** | **Sequence Number** | **Data** | **CRC** |
| --------- | ------------------- | -------- | ------- |
| 1 Byte    | 1 Byte              | 0+ Bytes | 2 Bytes |

- Flags - Link layer flags which contain metadata about the packet:
    - bit0 - Response - Set if this packet is in response to a prior request
    - bit1 - Direction - Set to 1 if this packet is sent by the Server
    - bit2 - LinkControl - Designates this as a LinkControl packet which is handled completely within the link layer and is not passed up to the application layer
    - bit3 - Reserved - Must be set to 0
    - bits7-4 - Version - SONAR version (currently 1)
- Sequence Number - A continuously increasing number which identifies a discrete request and its response
- CRC - A 16-bit CRC of all other bytes of the packet (excludes the CRC field)

## Connection

A connection is established at the link layer between the client and server through the following sequence:

1. The client sends a packet with the LinkControl flag set and 1 byte of data. The server should set its initial sequence number to this 1-byte data value.
2. The server responds with a packet which has the LinkControl flag set and no data.

Once a connection is established, SONAR maintains the connection by relying on a consistent stream of other (higher level) packets. If no higher level packets are sent for a configurable amount of time, the link layer may send a packet with the LinkControl flag set and no data to maintain the connection.

## Packet Exchange

After receiving a request, each endpoint should immediately send a response packet to acknowledge that it has received the request. This response must have the same sequence number as the request and have the response bit (bit0 of the flags) set. The response may optionally contain data, as specified by the application layer. Any responses received with an unexpected sequence number are silently discarded. The sender will wait up to a configurable timeout for the response, and then re-send the request a configurable number of times. If these timeouts and retries are exhausted, the link will become disconnected and the connection process will restart. Note that only one request may be outstanding at a time.

## Sequence Numbers

Every packet contains a sequence number as defined in the packet format above. The sequence number within a response packet is always equal to the sequence number from the request packet to which the response packet belongs. The sequence number set within a request packet is based on the current sequence number of the endpoint which is sending the request. This means that the sequence numbers used by each endpoint in their requests are independent from each other. Once a request is completed, that endpoint increments its sequence number such that the next request has a new sequence number (which is 1 higher than the previous sequence number - rolling over to 0 after 255). The result is that during reception of request packets, endpoints can know if they previously missed a request by comparing the sequence number within the request to the previous one which was processed. If the sequence number is the same as the previous one, this indicates that the request is a retry of the previous request.

When the server receives a connection request packet from the client, it should always reset its sequence number (and any other connection state) as specified in the connection process above. The server should also ignore any gap between the sequence number used for connection requests and the previous request sequence number which it saw. This allows the connection to be cleanly reestablished after the it is dropped (i.e. due to the client resetting).

There is a noteworthy situation where the server is sending requests to a client which has disconnected. As the client attempts to reconnect, it may receive a request before it receives the connection response from the server. In this case, it should silently drop these requests as it is not currently connected. The server should also stop sending (and fail) the request it was previously sending upon reception of the connection request from the client. Therefore, there is no case where the server sends a request to the client with a stale sequence number after the client believes a connection has been established (upon reception of the connection response).

# Application Layer

The application layer of SONAR is responsible for exposing attributes which are supported by the server to the client. There are a defined set of control attributes which are required to be implemented, with all other attributes being optional (as far as SONAR is concerned - the user is free to impose their own restrictions). Each attribute supports one or more operations. The list of supported attributes and their operations is predefined and static within a given connection.

## Packet Format

The application layer specifies the format of the data payload for requests.

| **Attribute ID** | **Data** |
| - | - |
| 2 Bytes | 0+ Bytes |

Responses only contain 0 or more bytes of data and not the attribute ID as the matching of requests and responses is handled by the link layer.

**Attribute ID**
The attribute ID is 16 bits and consists of the following fields:

- bits11-0 - A unique ID which identifies the attribute
- bits15-12 - The operation being performed on this attribute (Read=0x1, Write=0x2, Notify=0x3)

In order to simplify debugging, as a general (unenforced) convention, the top 4 bits of the 12-bit ID designate the version of the attribute, the next 4 bits designate the group which the attribute belongs to (0x0 are control attributes), and the bottom 4 bits designate the actual attribute.

## Operations

Each attribute must support one or more of the following operations.

### Read

The client may read an attribute on the server to get the current value. The request packet should contain no data, whereas the response packet’s data field should contain the current value of the attribute.

### Write

The client may write to an attribute on the server to set the current value. The request packet should contain the new value of the attribute. The response packet should contain no data.

### Notify

The server may notify the client that an attribute has changed. The request packet (sent by the server) should contain the new value of the attribute. The response packet should contain no data.

# Control Attributes

The following attributes must be supported by all SONAR servers.

| **Name** | **ID** | **Operations** | **Format** | **Description** |
| - | - | - | - | - |
| CTRL_NUM_ATTRS | 0x101 | Read | u16 | Contains the number of attributes which the server supports (excluding control attributes). |
| CTRL_ATTR_OFFSET | 0x102 | Read/Write | u16 | The current offset used to populate the CTRL_ATTR_LIST attribute. |
| CTRL_ATTR_LIST | 0x103 | Read | u16[8]; | The attribute IDs (not including these required control attributes) and their supported operations starting at an offset specified by the CTRL_ATTR_OFFSET attribute. The operations are encoded in the upper 4 bits:<br>  bit12: Read<br>  bit13: Write<br>  bit14: Notify<br>  bit15: Reserved (set to 0) |

NOTE: All other 12-bit attributes IDs of the form `0xh0h` (bits11-8 set to 0) are reserved for future use as control attributes.

# Attributes

Additional attributes are defined by the application. The only restriction is that the 12-bit attribute ID must not conflict with one of the control attributes. SONAR imposes no additional restrictions on the format or size of the application-defined attributes, including enforcing no requirement that the size is fixed within a connection.


