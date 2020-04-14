#include "buffer_chain.h"

void buffer_chain_set_data(buffer_chain_entry_t* entry, const uint8_t* data, uint32_t length) {
    entry->data = data;
    entry->length = length;
}

void buffer_chain_push_back(buffer_chain_entry_t* chain, buffer_chain_entry_t* to_insert) {
    while (chain->next) {
        chain = chain->next;
    }
    chain->next = to_insert;
}
