// We explicitly don't use include guards as this file should not be included recursively.

#include <inttypes.h>
#include <stdbool.h>

#ifdef _MSC_VER
#define _LOGGING_FORMAT_ATTR
#define _LOGGING_USED_ATTR
#else
#define _LOGGING_FORMAT_ATTR __attribute__((format(printf, 5, 6)))
#define _LOGGING_USED_ATTR __attribute__((used))
#endif

// The maximum length of a log message (not including extra formatting)
#ifndef LOGGING_MAX_MSG_LENGTH
#define LOGGING_MAX_MSG_LENGTH 128
#endif

// The application's build scripts can define FILENAME to the name of the file without the full path in order to save code space
#ifndef FILENAME
#define FILENAME __FILE__
#endif

// NOTE: LOGGING_MODULE_NAME can be defined before including this header in order to specify the module which the file belongs to

typedef enum {
    LOGGING_LEVEL_DEFAULT = 0, // Used to represent the default level specified to logging_init()
    LOGGING_LEVEL_DEBUG,
    LOGGING_LEVEL_INFO,
    LOGGING_LEVEL_WARN,
    LOGGING_LEVEL_ERROR,
} logging_level_t;

typedef struct {
    // Write function which gets passed a fully-formatted log line
    void(*write_function)(const char* str);
    // Write function which gets passed the level and module name split out in addition to the fully-formatted log line
    void(*raw_write_function)(logging_level_t level, const char* module_name, const char* str);
    // A lock function which is called to make the logging library thread-safe
    void(*lock_function)(bool acquire);
    // A function which is called to get the current system time in milliseconds
    uint32_t(*time_ms_function)(void);
    // The default logging level
    logging_level_t default_level;
} logging_init_t;

// Initialize the logging library
bool logging_init(const logging_init_t* init);

// Internal type used to represent a logger
typedef struct {
    uint8_t _private[sizeof(logging_level_t)];
    const char* const module_prefix;
} logging_logger_t;

// Change the logging threshold for the current module
#define LOG_SET_LEVEL(LEVEL) logging_module_set_level_impl(&_logging_logger, LEVEL)

// Macros for logging at each level
#define LOG_DEBUG(...) _LOG_LEVEL_IMPL(LOGGING_LEVEL_DEBUG, __VA_ARGS__)
#define LOG_INFO(...) _LOG_LEVEL_IMPL(LOGGING_LEVEL_INFO, __VA_ARGS__)
#define LOG_WARN(...) _LOG_LEVEL_IMPL(LOGGING_LEVEL_WARN, __VA_ARGS__)
#define LOG_ERROR(...) _LOG_LEVEL_IMPL(LOGGING_LEVEL_ERROR, __VA_ARGS__)

// Internal implementation macros / functions which are called via the macros above
#define _LOG_LEVEL_IMPL(LEVEL, ...) logging_log_impl(&_logging_logger, LEVEL, FILENAME, __LINE__, __VA_ARGS__)
void logging_log_impl(logging_logger_t* logger, logging_level_t level, const char* file, int line, const char* fmt, ...) _LOGGING_FORMAT_ATTR;
void logging_set_level_impl(logging_logger_t* logger, logging_level_t level);

// Per-file context object which we should create
static logging_logger_t _logging_logger _LOGGING_USED_ATTR = {
#ifdef LOGGING_MODULE_NAME
    .module_prefix = LOGGING_MODULE_NAME ":",
#else
    .module_prefix = 0,
#endif
};
