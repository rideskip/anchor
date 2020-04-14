#pragma once

// How long before we disconnect if we haven't received a response. A connection maintenance
// message is sent by the client at half this interval.
#define CONNECTION_TIMEOUT_MS               1000
#define CONNECTION_MAINTENANCE_INTERVAL_MS  500
#define REQUEST_RETRY_INTERVAL_MS           100
#define REQUEST_TIMEOUT_MS                  300

// Make sure that we have enough time to send a connection maintenance request and get the
// response before disconnecting, using the retry interval as an extra buffer.
#if CONNECTION_TIMEOUT_MS < CONNECTION_MAINTENANCE_INTERVAL_MS + REQUEST_TIMEOUT_MS + REQUEST_RETRY_INTERVAL_MS
#error "The connection timeout must be at least double the request timeout"
#endif
