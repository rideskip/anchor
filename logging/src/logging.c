#include "anchor/logging/logging.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define LEVEL_PREFIX_LENGTH     6
#define TIME_LENGTH             16
#define FILE_NAME_LENGTH        32
#define FULL_LOG_MAX_LENGTH     (LOGGING_MAX_MSG_LENGTH + LEVEL_PREFIX_LENGTH + TIME_LENGTH + FILE_NAME_LENGTH + 1)

#define GET_IMPL(LOGGER) ((logger_impl_t*)LOGGER->_private)

typedef struct {
    logging_level_t level;
} logger_impl_t;
_Static_assert(sizeof(logger_impl_t) == sizeof(((logging_logger_t*)0)->_private), "Invalid context size");

static const char* LEVEL_PREFIX[] = {
    [LOGGING_LEVEL_DEFAULT] =   "????? ", // should never be printed
    [LOGGING_LEVEL_DEBUG] =     "DEBUG ",
    [LOGGING_LEVEL_INFO] =      "INFO  ",
    [LOGGING_LEVEL_WARN] =      "WARN  ",
    [LOGGING_LEVEL_ERROR] =     "ERROR ",
};

static logging_init_t m_init;
static char m_line_time_buffer[TIME_LENGTH];
static char m_write_buffer[FULL_LOG_MAX_LENGTH];

bool logging_init(const logging_init_t* init) {
    if ((!init->write_function && !init->raw_write_function) || init->default_level == LOGGING_LEVEL_DEFAULT) {
        return false;
    }
    m_init = *init;
    return true;
}

void logging_log_impl(logging_logger_t* logger, logging_level_t level, const char* file, int line, const char* fmt, ...) {
    logger_impl_t* impl = GET_IMPL(logger);
    const logging_level_t min_level = impl->level == LOGGING_LEVEL_DEFAULT ? m_init.default_level : impl->level;
    if (level < min_level) {
        return;
    }

    if (m_init.lock_function) {
        m_init.lock_function(true);
    }

    // time
    if (m_init.time_ms_function) {
        uint32_t current_time = m_init.time_ms_function();
        const uint16_t ms = current_time % 1000;
        current_time /= 1000;
        const uint16_t sec = current_time % 60;
        current_time /= 60;
        const uint16_t min = current_time % 60;
        current_time /= 60;
        const uint16_t hours = current_time;
        snprintf(m_write_buffer, FULL_LOG_MAX_LENGTH, "%3u:%02u:%02u.%03u ", hours, min, sec, ms);
    }

    // level
    snprintf(&m_write_buffer[strlen(m_write_buffer)], FULL_LOG_MAX_LENGTH-strlen(m_write_buffer), "%s", LEVEL_PREFIX[level]);

    // module (if set)
    if (logger->module_prefix) {
        snprintf(&m_write_buffer[strlen(m_write_buffer)], FULL_LOG_MAX_LENGTH-strlen(m_write_buffer), "%s", logger->module_prefix);
    }

    // file name
    snprintf(&m_write_buffer[strlen(m_write_buffer)], FULL_LOG_MAX_LENGTH-strlen(m_write_buffer), "%s", file);

    // line number
    snprintf(m_line_time_buffer, sizeof(m_line_time_buffer), ":%d: ", line);
    snprintf(&m_write_buffer[strlen(m_write_buffer)], FULL_LOG_MAX_LENGTH-strlen(m_write_buffer), ":%d: ", line);

    // log message
    va_list args;
    va_start(args, fmt);
    vsnprintf(&m_write_buffer[strlen(m_write_buffer)], FULL_LOG_MAX_LENGTH-strlen(m_write_buffer), fmt, args);
    va_end(args);

    // newline
    snprintf(&m_write_buffer[strlen(m_write_buffer)], FULL_LOG_MAX_LENGTH-strlen(m_write_buffer), "\n");

    if (m_init.write_function) {
        m_init.write_function(m_write_buffer);
    }
    if (m_init.raw_write_function) {
        m_init.raw_write_function(level, logger->module_prefix, m_write_buffer);
    }

    if (m_init.lock_function) {
        m_init.lock_function(false);
    }
}

void logging_set_level_impl(logging_logger_t* logger, logging_level_t level) {
    GET_IMPL(logger)->level = level;
}
