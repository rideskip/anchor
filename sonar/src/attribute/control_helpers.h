#pragma once

#include <inttypes.h>

#define CTRL_ATTR_LIST_OP_BIT_READ      (1 << 12)
#define CTRL_ATTR_LIST_OP_BIT_WRITE     (1 << 13)
#define CTRL_ATTR_LIST_OP_BIT_NOTIFY    (1 << 14)
#define CTRL_ATTR_LIST_OP_BIT_RESERVED  (1 << 15)

#define CTRL_ATTR_LIST_LENGTH           8
typedef uint16_t ctrl_attr_list_t[CTRL_ATTR_LIST_LENGTH];

#define CTRL_NUM_ATTRS_ID               0x101
#define CTRL_ATTR_OFFSET_ID             0x102
#define CTRL_ATTR_LIST_ID               0x103

#define CTRL_NUM_ATTRS_TYPE             uint16_t
#define CTRL_ATTR_OFFSET_TYPE           uint16_t
#define CTRL_ATTR_LIST_TYPE             ctrl_attr_list_t
