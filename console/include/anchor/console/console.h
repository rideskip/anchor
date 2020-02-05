#pragma once

#include "anchor/console/console_config.h"
#include "anchor/console/console_private_helpers.h"

#include <inttypes.h>
#include <stdbool.h>

#define CONSOLE_INT_ARG_DEFAULT ((intptr_t)-1)
#define CONSOLE_STR_ARG_DEFAULT ((const char*)NULL)

// Generic command handler type which is used internally by the console library
typedef void(*console_command_handler_t)(const void*);

typedef enum {
    // An integer argument (of type "intptr_t")
    CONSOLE_ARG_TYPE_INT,
    // A string argument
    CONSOLE_ARG_TYPE_STR,
} console_arg_type_t;

typedef struct {
    // The name of the argument
    const char* name;
#if CONSOLE_HELP_COMMAND
    // A description of the argument for display by the "help" command
    const char* desc;
#endif
    // The type of the argument
    console_arg_type_t type;
    // Whether or not the arg is optional (can ONLY be set for the last argument)
    bool is_optional;
} console_arg_def_t;

typedef struct {
    // The name of the command (must be a valid C symbol name, consist of only lower-case alphanumeric characters, and be unique)
    const char* name;
#if CONSOLE_HELP_COMMAND
    // A description of the command for display by the "help" command
    const char* desc;
#endif
    // The command handler
    console_command_handler_t handler;
    // List of argument definitions
    const console_arg_def_t* args;
    // The number of arguments
    uint32_t num_args;
    // A pointer which the console library uses internally to store arguments for this command
    void** args_ptr;
} console_command_def_t;

typedef struct {
    // Write function which gets passed a string to be written out
    void(*write_function)(const char* str);
} console_init_t;

// Defines a console command
#if CONSOLE_HELP_COMMAND
#define CONSOLE_COMMAND_DEF(CMD, DESC, ...) \
    typedef struct { \
        _CONSOLE_MAP(_CONSOLE_ARG_TYPE_WITH_DESC_HELPER, ##__VA_ARGS__) \
        const void* const __dummy; /* dummy entry so the struct isn't empty */ \
    } CMD##_args_t; \
    _Static_assert(sizeof(CMD##_args_t) == (_CONSOLE_NUM_ARGS(__VA_ARGS__) + 1) * sizeof(void *), \
        "Compiler created *_args_t struct of unexpected size"); \
    static void CMD##_command_handler(const CMD##_args_t* args); \
    static const console_arg_def_t _##CMD##_ARGS_DEF[] = { \
        _CONSOLE_MAP(_CONSOLE_ARG_DEF_WITH_DESC_HELPER, ##__VA_ARGS__) \
    }; \
    static void* _##CMD##_ARGS[_CONSOLE_NUM_ARGS(__VA_ARGS__)]; \
    static const console_command_def_t _##CMD##_DEF = { \
        .name = #CMD, \
        .desc = DESC, \
        .handler = (console_command_handler_t)CMD##_command_handler, \
        .args = _##CMD##_ARGS_DEF, \
        .num_args = sizeof(_##CMD##_ARGS_DEF) / sizeof(console_arg_def_t), \
        .args_ptr = _##CMD##_ARGS, \
    }; \
    static const console_command_def_t* const CMD = &_##CMD##_DEF
#else
#define CONSOLE_COMMAND_DEF(CMD, ...) \
    static void CMD##_command_handler(void); \
    static const console_arg_def_t _##CMD##_ARGS[] = { \
        _CONSOLE_MAP(_CONSOLE_ARG_DEF_HELPER, ##__VA_ARGS__) \
    }; \
    static const console_command_def_t _##CMD##_DEF = { \
        .name = #CMD, \
        .handler = CMD##_command_handler, \
        .args = _##CMD##_ARGS, \
        .num_args = sizeof(_##CMD##_ARGS) / sizeof(console_arg_def_t), \
    }; \
    static const console_command_def_t* const CMD = &_##CMD##_DEF
#endif

// Defines an integer argument of a console command
#if CONSOLE_HELP_COMMAND
#define CONSOLE_INT_ARG_DEF(NAME, DESC) (NAME, DESC, CONSOLE_ARG_TYPE_INT, false, intptr_t)
#else
#define CONSOLE_INT_ARG_DEF(NAME) (NAME, CONSOLE_ARG_TYPE_INT, false, intptr_t)
#endif

// Defines a string argument of a console command
#if CONSOLE_HELP_COMMAND
#define CONSOLE_STR_ARG_DEF(NAME, DESC) (NAME, DESC, CONSOLE_ARG_TYPE_STR, false, const char*)
#else
#define CONSOLE_STR_ARG_DEF(NAME) (NAME, CONSOLE_ARG_TYPE_STR, false, const char*)
#endif

// Defines an optional integer argument of a console command (can ONLY be used for the last argument)
// The argument will have a value of `CONSOLE_INT_ARG_DEFAULT` when not specified
#if CONSOLE_HELP_COMMAND
#define CONSOLE_OPTIONAL_INT_ARG_DEF(NAME, DESC) (NAME, DESC, CONSOLE_ARG_TYPE_INT, true, intptr_t)
#else
#define CONSOLE_OPTIONAL_INT_ARG_DEF(NAME) (NAME, CONSOLE_ARG_TYPE_INT, true, intptr_t)
#endif

// Defines an optional string argument of a console command (can ONLY be used for the last argument)
// The argument will have a value of `CONSOLE_STR_ARG_DEFAULT` when not specified
#if CONSOLE_HELP_COMMAND
#define CONSOLE_OPTIONAL_STR_ARG_DEF(NAME, DESC) (NAME, DESC, CONSOLE_ARG_TYPE_STR, true, const char*)
#else
#define CONSOLE_OPTIONAL_STR_ARG_DEF(NAME) (NAME, CONSOLE_ARG_TYPE_STR, true, const char*)
#endif

// Initializes the console library
void console_init(const console_init_t* init);

// Registers a console command with the console library (returns true on success)
bool console_command_register(const console_command_def_t* cmd);

// Processes received data
void console_process(const uint8_t* data, uint32_t length);

#if CONSOLE_FULL_CONTROL
// Prints a string (should end with a '\n') without visibly corrupting the current command line
void console_print_line(const char* str);
#endif
