#include "anchor/console/console.h"

#include <limits.h>
#include <string.h>
#include <stdlib.h>

#define CHAR_CTRL_C     0x03

typedef union {
    intptr_t value_int;
    const char* value_str;
    void* ptr;
} parsed_arg_t;

#if CONSOLE_HELP_COMMAND
CONSOLE_COMMAND_DEF(help, "List all commands, or give details about a specific command",
    CONSOLE_OPTIONAL_STR_ARG_DEF(command, "The name of the command to give details about")
);
#endif

static console_init_t m_init;
static console_command_def_t m_commands[CONSOLE_MAX_COMMANDS] CONSOLE_BUFFER_ATTRIBUTES;
static uint32_t m_num_commands;
static char m_line_buffer[CONSOLE_MAX_LINE_LENGTH] CONSOLE_BUFFER_ATTRIBUTES;
static uint32_t m_line_len;
static bool m_line_invalid;
static bool m_is_active;
static uint32_t m_escape_sequence_index = 0;
#if CONSOLE_HISTORY
static char m_history_buffer[CONSOLE_HISTORY][CONSOLE_MAX_LINE_LENGTH] CONSOLE_BUFFER_ATTRIBUTES;
static uint32_t m_history_start_index = 0;
static uint32_t m_history_len = 0;
static int32_t m_history_index = -1;
#endif

static bool validate_arg_def(const console_arg_def_t* arg, bool is_last) {
    switch (arg->type) {
        case CONSOLE_ARG_TYPE_INT:
        case CONSOLE_ARG_TYPE_STR:
            return arg->name && (!arg->is_optional || is_last);
        default:
            return false;
    }
}

static const console_command_def_t* get_command(const char* name) {
    for (uint32_t i = 0; i < m_num_commands; i++) {
        if (!strcmp(m_commands[i].name, name)) {
            return &m_commands[i];
        }
    }
    return NULL;
}

static void write_str(const char* str) {
    if (!m_init.write_function) {
        return;
    }
    m_init.write_function(str);
}

static bool parse_arg(const char* arg_str, console_arg_type_t type, parsed_arg_t* parsed_arg) {
    switch (type) {
        case CONSOLE_ARG_TYPE_INT: {
            char* end_ptr = NULL;
            long int result = strtol(arg_str, &end_ptr, 0);
            if (result == LONG_MAX || result == LONG_MIN || end_ptr == NULL || *end_ptr != '\0') {
                return false;
            }
            parsed_arg->value_int = (intptr_t)result;
            return true;
        }
        case CONSOLE_ARG_TYPE_STR:
            parsed_arg->value_str = arg_str;
            return true;
        default:
            // should never get here
            return false;
    }
}

static uint32_t get_num_required_args(const console_command_def_t* cmd) {
    if (cmd->num_args == 0) {
        return 0;
    }
    const console_arg_def_t* last_arg = &cmd->args[cmd->num_args-1];
    switch (last_arg->type) {
        case CONSOLE_ARG_TYPE_INT:
        case CONSOLE_ARG_TYPE_STR:
            if (last_arg->is_optional) {
                return cmd->num_args - 1;
            } else {
                return cmd->num_args;
            }
        default:
            return 0;
    }
}

static void process_line(void) {
    if (m_line_invalid) {
        return;
    }

    // parse and validate the line
    const console_command_def_t* cmd = NULL;
    uint32_t arg_index = 0;
    const char* current_token = NULL;
    for (uint32_t i = 0; i <= m_line_len; i++) {
        const char c = m_line_buffer[i];
        if (c == ' ' || c == '\0') {
            // end of a token
            if (!current_token) {
                if (cmd) {
                    // too much whitespace
                    write_str("ERROR: Extra whitespace between arguments\n");
                    return;
                } else {
                    // empty line - silently fail
                    return;
                }
            }
#if CONSOLE_HISTORY
            if (!cmd && strcmp(m_history_buffer[(m_history_start_index + m_history_len) % CONSOLE_HISTORY], m_line_buffer)) {
                // add this line to our history since it contains at least one token and is different from the previous history line
                const uint32_t history_write_index = (m_history_start_index + m_history_len + 1) % CONSOLE_HISTORY;
                strcpy(m_history_buffer[history_write_index], m_line_buffer);
                if (m_history_len == CONSOLE_HISTORY) {
                    // history buffer is full, so drop the first element to clear room for the new one
                    m_history_start_index = (m_history_start_index + 1) % CONSOLE_HISTORY;
                } else {
                    m_history_len++;
                }
            }
#endif
            // process this token
            m_line_buffer[i] = '\0';
            if (!cmd) {
                // find the command
                cmd = get_command(current_token);
                if (!cmd) {
                    write_str("ERROR: Command not found (");
                    write_str(current_token);
                    write_str(")\n");
                    return;
                }
            } else {
                // this is an argument
                if (arg_index == cmd->num_args) {
                    write_str("ERROR: Too many arguments\n");
                    return;
                }
                // validate the argument
                const console_arg_def_t* arg = &cmd->args[arg_index];
                parsed_arg_t parsed_arg;
                if (!parse_arg(current_token, arg->type, &parsed_arg)) {
                    write_str("ERROR: Invalid value for '");
                    write_str(arg->name);
                    write_str("' (");
                    write_str(current_token);
                    write_str(")\n");
                    return;
                }
                cmd->args_ptr[arg_index] = parsed_arg.ptr;
                arg_index++;
            }
            current_token = NULL;
        } else if (!current_token) {
            current_token = &m_line_buffer[i];
        }
    }

    if (!cmd) {
        // nothing entered - silently fail
        return;
    } else if (arg_index < get_num_required_args(cmd)) {
        write_str("ERROR: Too few arguments\n");
        return;
    }

    if (arg_index != cmd->num_args) {
        // set the optional argument to its default value
        switch (cmd->args[arg_index].type) {
            case CONSOLE_ARG_TYPE_INT:
                cmd->args_ptr[arg_index] = (void*)CONSOLE_INT_ARG_DEFAULT;
                break;
            case CONSOLE_ARG_TYPE_STR:
                cmd->args_ptr[arg_index] = (void*)CONSOLE_STR_ARG_DEFAULT;
                break;
        }
    }

    // run the handler
    arg_index = 0;
    m_is_active = true;
    cmd->handler(cmd->args_ptr);
    m_is_active = false;
}

static void reset_line_and_print_prompt(void) {
#if CONSOLE_HISTORY
    m_history_index = -1;
#endif
    m_escape_sequence_index = 0;
    m_line_len = 0;
    m_line_invalid = false;
    m_line_buffer[0] = '\0';
    write_str(CONSOLE_PROMPT);
}

static void push_char(char c) {
    m_line_buffer[m_line_len++] = c;
    m_line_buffer[m_line_len] = '\0';
    if (m_line_len == CONSOLE_MAX_LINE_LENGTH) {
        // filled up the line buffer, so mark the line invalid
        m_line_invalid = true;
    }
}

#if CONSOLE_FULL_CONTROL
static void erase_current_line(uint32_t new_line_line) {
    if (new_line_line < m_line_len) {
        const uint32_t char_to_erase = m_line_len - new_line_line;
        for (uint32_t i = 0; i < char_to_erase; i++) {
            write_str("\b");
        }
        for (uint32_t i = 0; i < char_to_erase; i++) {
            write_str(" ");
        }
    }
    write_str("\r");
}
#endif

#if CONSOLE_TAB_COMPLETE
static void do_tab_complete(void) {
    uint32_t num_matches = 0;
    uint32_t longest_common_prefix = 0;
    const console_command_def_t* first_cmd_def = NULL;
    for (uint32_t i = 0; i < m_num_commands; i++) {
        const console_command_def_t* cmd_def = &m_commands[i];
        if (strncmp(cmd_def->name, m_line_buffer, m_line_len)) {
            continue;
        }
        if (num_matches > 0) {
            for (uint32_t j = 0; j < longest_common_prefix; j++) {
                if (cmd_def->name[j] != first_cmd_def->name[j]) {
                    longest_common_prefix = j;
                    break;
                }
            }
        } else {
            first_cmd_def = cmd_def;
            longest_common_prefix = strlen(cmd_def->name);
        }
        num_matches++;
    }
    if (num_matches == 0) {
        return;
    }

    if (num_matches == 1) {
        // a single match, so add on the remainder of the command
        const uint32_t prev_line_len = m_line_len;
        for (uint32_t i = m_line_len; i < longest_common_prefix; i++) {
            m_line_buffer[m_line_len++] = first_cmd_def->name[i];
        }
        m_line_buffer[m_line_len] = '\0';
        write_str(&m_line_buffer[prev_line_len]);
    } else if (longest_common_prefix == m_line_len) {
        // multiple matches and we've already entered the longest common prefix,
        // so print all the potential matches as a new line
        write_str("\n");
        for (uint32_t i = 0; i < m_num_commands; i++) {
            const console_command_def_t* cmd_def = &m_commands[i];
            if (strncmp(cmd_def->name, m_line_buffer, m_line_len)) {
                continue;
            }
            if (cmd_def != first_cmd_def) {
                write_str(" ");
            }
            write_str(cmd_def->name);
        }
        write_str("\n");
        // re-print the prompt and any valid, pending command
        write_str(CONSOLE_PROMPT);
        if (!m_line_invalid) {
            write_str(m_line_buffer);
        }
    } else {
        // multiple matches and there's a longer common prefix than what's already typed,
        // so add the rest of it on to the current command
        const uint32_t prev_line_len = m_line_len;
        for (uint32_t i = m_line_len; i < longest_common_prefix; i++) {
            m_line_buffer[m_line_len++] = first_cmd_def->name[i];
        }
        m_line_buffer[m_line_len] = '\0';
        write_str(&m_line_buffer[prev_line_len]);
    }
}
#endif

#if CONSOLE_HELP_COMMAND
static void help_command_handler(const help_args_t* args) {
    if (args->command != CONSOLE_STR_ARG_DEFAULT) {
        const console_command_def_t* cmd_def = get_command(args->command);
        if (!cmd_def) {
            write_str("ERROR: Unknown command (");
            write_str(args->command);
            write_str(")\n");
            return;
        }
        if (cmd_def->desc) {
            write_str(cmd_def->desc);
            write_str("\n");
        }
        write_str("Usage: ");
        write_str(args->command);
        uint32_t max_name_len = 0;
        for (uint32_t i = 0; i < cmd_def->num_args; i++) {
            const console_arg_def_t* arg_def = &cmd_def->args[i];
            const uint32_t name_len = strlen(arg_def->name);
            if (name_len > max_name_len) {
                max_name_len = name_len;
            }
            if (arg_def->is_optional) {
                write_str(" [");
            } else {
                write_str(" ");
            }
            write_str(cmd_def->args[i].name);
            if (arg_def->is_optional) {
                write_str("]");
            }
        }
        write_str("\n");
        for (uint32_t i = 0; i < cmd_def->num_args; i++) {
            const console_arg_def_t* arg_def = &cmd_def->args[i];
            write_str("  ");
            write_str(arg_def->name);
            if (arg_def->desc) {
                // pad the description so they all line up
                const uint32_t name_len = strlen(arg_def->name);
                for (uint32_t j = 0; j < max_name_len - name_len; j++) {
                    write_str(" ");
                }
                write_str(" - ");
                write_str(arg_def->desc);
            }
            write_str("\n");
        }
    } else {
        write_str("Available commands:\n");
        // get the max name length for padding
        uint32_t max_name_len = 0;
        for (uint32_t i = 0; i < m_num_commands; i++) {
            const console_command_def_t* cmd_def = &m_commands[i];
            const uint32_t name_len = strlen(cmd_def->name);
            if (name_len > max_name_len) {
                max_name_len = name_len;
            }
        }
        for (uint32_t i = 0; i < m_num_commands; i++) {
            const console_command_def_t* cmd_def = &m_commands[i];
            write_str("  ");
            write_str(cmd_def->name);
            if (cmd_def->desc) {
                // pad the description so they all line up
                const uint32_t name_len = strlen(cmd_def->name);
                for (uint32_t j = 0; j < max_name_len - name_len; j++) {
                    write_str(" ");
                }
                write_str(" - ");
                write_str(cmd_def->desc);
            }
            write_str("\n");
        }
    }
}
#endif

void console_init(const console_init_t* init) {
    m_init = *init;
#if CONSOLE_HELP_COMMAND
    console_command_register(help);
#endif
    write_str("\n" CONSOLE_PROMPT);
}

bool console_command_register(const console_command_def_t* cmd) {
    if (m_num_commands == CONSOLE_MAX_COMMANDS) {
        return false;
    }
    // validate the command
    if (!cmd->name || !cmd->handler || strlen(cmd->name) >= CONSOLE_MAX_LINE_LENGTH) {
        return false;
    }
    // validate the arguments
    for (uint32_t i = 0; i < cmd->num_args; i++) {
        if (!validate_arg_def(cmd->args, i + 1 == cmd->num_args)) {
            return false;
        }
    }
    // make sure it's not already registered
    if (get_command(cmd->name)) {
        return false;
    }
    // add the command
    m_commands[m_num_commands++] = *cmd;
    return true;
}

void console_process(const uint8_t* data, uint32_t length) {
#if CONSOLE_FULL_CONTROL
    const char* echo_str = NULL;
    for (uint32_t i = 0; i < length; i++) {
        const char c = data[i];
        if (m_escape_sequence_index == 0 && c == '\x1b') {
            // start of an escape sequence
            m_escape_sequence_index++;
            continue;
        } else if (m_escape_sequence_index == 1) {
            if (c == '[') {
                m_escape_sequence_index++;
            } else {
                // invalid escape sequence
                m_escape_sequence_index = 0;
            }
            continue;
        } else if (m_escape_sequence_index == 2) {
            // process the command
            m_escape_sequence_index = 0;
#if CONSOLE_HISTORY
            bool update_line_from_history = false;
            if (c == 'A') {
                // up arrow
                if (m_history_index + 1 < m_history_len) {
                    m_history_index++;
                    update_line_from_history = true;
                }
            } else if (c == 'B') {
                // down arrow
                if (m_history_index > -1) {
                    m_history_index--;
                    update_line_from_history = true;
                }
            }
            if (update_line_from_history) {
                const char* history_line = (m_history_index == -1) ? "" : m_history_buffer[(m_history_start_index + m_history_len - m_history_index) % CONSOLE_HISTORY];
                erase_current_line(strlen(history_line));
                strcpy(m_line_buffer, history_line);
                m_line_len = strlen(m_line_buffer);
                write_str(CONSOLE_PROMPT);
                write_str(m_line_buffer);
            }
#endif
            continue;
        }
        if (c == '\n') {
            if (echo_str) {
                write_str(echo_str);
                echo_str = NULL;
            }
            write_str("\n");
            process_line();
            reset_line_and_print_prompt();
        } else if (c == CHAR_CTRL_C) {
            if (echo_str) {
                write_str(echo_str);
                echo_str = NULL;
            }
            write_str("\n");
            reset_line_and_print_prompt();
            echo_str = NULL;
        } else if (!m_line_invalid && c == '\b') {
            if (echo_str) {
                write_str(echo_str);
                echo_str = NULL;
            }
            if (m_line_len) {
                write_str("\b \b");
                m_line_buffer[--m_line_len] = '\0';
            }
#if CONSOLE_TAB_COMPLETE
        } else if (!m_line_invalid && c == '\t') {
            if (echo_str) {
                write_str(echo_str);
                echo_str = NULL;
            }
            do_tab_complete();
#endif
        } else if (!m_line_invalid && c >= ' ' && c <= '~') {
            // valid character
            if (!echo_str) {
                echo_str = &m_line_buffer[m_line_len];
            }
            push_char(c);
        }
    }
    if (echo_str) {
        write_str(echo_str);
    }
#else
    for (uint32_t i = 0; i < length; i++) {
        const char c = data[i];
        if (c == '\n') {
            process_line();
            reset_line_and_print_prompt();
        } else if (!m_line_invalid && c >= ' ' && c <= '~') {
            // valid character
            push_char(c);
        } else {
            m_line_invalid = true;
        }
    }
#endif
}

#if CONSOLE_FULL_CONTROL
void console_print_line(const char* str) {
    // erase any characters which would end up still showing after the end line
    erase_current_line(strlen(str));
    // print the line
    write_str(str);
    if (!m_is_active) {
        // re-print the prompt and any valid, pending command
        write_str(CONSOLE_PROMPT);
        if (!m_line_invalid) {
            write_str(m_line_buffer);
        }
    }
}
#endif
