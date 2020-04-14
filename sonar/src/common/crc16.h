#pragma once

#include <inttypes.h>

#define CRC16_INITIAL_VALUE UINT16_C(0xffff)

uint16_t crc16(const uint8_t* data, uint32_t length, uint16_t crc);
