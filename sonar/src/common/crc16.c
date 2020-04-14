#include "crc16.h"

uint16_t crc16(const uint8_t* data, uint32_t length, uint16_t crc) {
    for (uint32_t i = 0; i < length; i++) {
        uint8_t x = (crc >> 8) ^ data[i];
        x ^= x >> 4;
        crc = (crc << 8) ^ (x << 12) ^ (x << 5) ^ x;
    }
    return crc;
}
