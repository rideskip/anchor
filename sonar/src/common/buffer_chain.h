#pragma once

#include <inttypes.h>

#define FOREACH_BUFFER_CHAIN_ENTRY(START_PTR, ENTRY_PTR_NAME) \
    for (const buffer_chain_entry_t* ENTRY_PTR_NAME = START_PTR; ENTRY_PTR_NAME; ENTRY_PTR_NAME = (ENTRY_PTR_NAME)->next)

typedef struct buffer_chain_entry {
    // Next entry in the chain
    struct buffer_chain_entry* next;
    // The data buffer represented by this entry
    const uint8_t* data;
    // The length of the data buffer represented by this entry
    uint32_t length;
} buffer_chain_entry_t;

// Sets the data represented by a buffer chain entry
void buffer_chain_set_data(buffer_chain_entry_t* entry, const uint8_t* data, uint32_t length);

// Inserts a new entry at the end of the chain
void buffer_chain_push_back(buffer_chain_entry_t* chain, buffer_chain_entry_t* to_insert);
