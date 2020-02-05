#pragma once

#if defined(__GNUC__)

// The _CONSOLE_NUM_ARGS(...) macro returns the number of arguments passed to it (0-10)
#define _CONSOLE_NUM_ARGS(...) _CONSOLE_NUM_ARGS_HELPER1(_, ##__VA_ARGS__, _CONSOLE_NUM_ARGS_SEQ())
#define _CONSOLE_NUM_ARGS_HELPER1(...) _CONSOLE_NUM_ARGS_HELPER2(__VA_ARGS__)
#define _CONSOLE_NUM_ARGS_HELPER2(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,N,...) N
#define _CONSOLE_NUM_ARGS_SEQ() 9,8,7,6,5,4,3,2,1,0
_Static_assert(_CONSOLE_NUM_ARGS() == 0, "_CONSOLE_NUM_ARGS() != 0");
_Static_assert(_CONSOLE_NUM_ARGS(1) == 1, "_CONSOLE_NUM_ARGS(1) != 1");
_Static_assert(_CONSOLE_NUM_ARGS(1,2,3,4,5,6,7,8,9,10) == 10, "_CONSOLE_NUM_ARGS(<10_args>) != 10");

// General helper macro for concatentating two tokens
#define _CONSOLE_CONCAT(A, B) _CONSOLE_CONCAT2(A, B)
#define _CONSOLE_CONCAT2(A, B) A ## B

// Helper macro for calling a given macro for each of 0-10 arguments
#define _CONSOLE_MAP(X, ...) _CONSOLE_MAP_HELPER(_CONSOLE_CONCAT(_CONSOLE_MAP_, _CONSOLE_NUM_ARGS(__VA_ARGS__)), X, ##__VA_ARGS__)
#define _CONSOLE_MAP_HELPER(C, X, ...) C(X, ##__VA_ARGS__)
#define _CONSOLE_MAP_0(X)
#define _CONSOLE_MAP_1(X,_1) X(_1)
#define _CONSOLE_MAP_2(X,_1,_2) X(_1) X(_2)
#define _CONSOLE_MAP_3(X,_1,_2,_3) X(_1) X(_2) X(_3)
#define _CONSOLE_MAP_4(X,_1,_2,_3,_4) X(_1) X(_2) X(_3) X(_4)
#define _CONSOLE_MAP_5(X,_1,_2,_3,_4,_5) X(_1) X(_2) X(_3) X(_4) X(_5)
#define _CONSOLE_MAP_6(X,_1,_2,_3,_4,_5,_6) X(_1) X(_2) X(_3) X(_4) X(_5) X(_6)
#define _CONSOLE_MAP_7(X,_1,_2,_3,_4,_5,_6,_7) X(_1) X(_2) X(_3) X(_4) X(_5) X(_6) X(_7)
#define _CONSOLE_MAP_8(X,_1,_2,_3,_4,_5,_6,_7,_8) X(_1) X(_2) X(_3) X(_4) X(_5) X(_6) X(_7) X(_8)
#define _CONSOLE_MAP_9(X,_1,_2,_3,_4,_5,_6,_7,_8,_9) X(_1) X(_2) X(_3) X(_4) X(_5) X(_6) X(_7) X(_8) X(_9)
#define _CONSOLE_MAP_10(X,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10) X(_1) X(_2) X(_3) X(_4) X(_5) X(_6) X(_7) X(_8) X(_9) X(_10)

// Helper macros for defining console_arg_def_t instances.
#define _CONSOLE_ARG_DEF_HELPER(...) _CONSOLE_ARG_DEF_HELPER2 __VA_ARGS__
#define _CONSOLE_ARG_DEF_HELPER2(NAME, DESC, ENUM_TYPE, IS_OPTIONAL, C_TYPE) \
    { .name = #NAME, .type = ENUM_TYPE, .is_optional = IS_OPTIONAL },
#define _CONSOLE_ARG_DEF_WITH_DESC_HELPER(...) _CONSOLE_ARG_DEF_WITH_DESC_HELPER2 __VA_ARGS__
#define _CONSOLE_ARG_DEF_WITH_DESC_HELPER2(NAME, DESC, ENUM_TYPE, IS_OPTIONAL, C_TYPE) \
    { .name = #NAME, .desc = DESC, .type = ENUM_TYPE, .is_optional = IS_OPTIONAL },

// Helper macros for defining command *_arg_t types.
#define _CONSOLE_ARG_TYPE_WITH_DESC_HELPER(...) _CONSOLE_ARG_TYPE_WITH_DESC_HELPER2 __VA_ARGS__
#define _CONSOLE_ARG_TYPE_WITH_DESC_HELPER2(NAME, DESC, ENUM_TYPE, IS_OPTIONAL, C_TYPE) \
    C_TYPE NAME;

#else // !defined(__GNUC__)
#error "Unsupported toolchain"
#endif
