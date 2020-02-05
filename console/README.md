# Console

A command-line console library which can be used to easily expose debug and
test commands via a serial interface.

## Features

The console library supports the following high-level features:
* Built-in validation of command arguments
* Auto-generated "help" command which lists all available commands, and can be
used to display usage information about specific commands (when enabled)
* Support for string and integer positional arguments, including (limited)
optional arguments
* Optimized for embedded environments with static memory allocation and the
ability to store command definitions in program memory (instead of RAM)
* Simple and easy to integrate API

## Usage

The first step is to define a console command and implement its handler. The
easiest way to do this is by using the `CONSOLE_COMMAND_DEF()` macro (see the
example below). During program initialization, `console_init()` should be
called to initialize the console library, and each command should be registered
with the console library using `console_command_register()`.

When the program receives data which should be interpreted by the console
library, it should be passed to `console_process()`. Note that the data passed
may contain a partial, complete, or even multiple commands. As part of this
call, the console library will call the command handlers, as appropriate.

## Compile Options

Parameters and additional features of the console library can be configured via
the following defines:
* `CONSOLE_MAX_COMMANDS` - The maximum number of console commands which may be
registered (default of 16).
* `CONSOLE_MAX_LINE_LENGTH` - The maximum length of a single line sent to the
console library (default of 128).
* `CONSOLE_PROMPT` - The string displayed to prompt for console input (default
of "> ").
* `CONSOLE_BUFFER_ATTRIBUTES` - Extra GCC attributes to add to the internal
buffers used by the console library (default of none). This can be used to if
these buffers should be placed in a different linker section, for example.
* `CONSOLE_HELP_COMMAND` - Enables the built-in help command (default of 1).
This can be disabled to save code space. Disabling this will also remove the
`desc` attribute of commands and arguments.
* `CONSOLE_FULL_CONTROL` - Enables more advanced line control features such as
backspace and CTRL-C (default of 1). Also enables the `console_print_line()`
function. This setting also results in the console echo'ing back any valid
characters it receives.
* `CONSOLE_TAB_COMPLETE` - Enables tab-completion of console commands (default
of 1). This depends on `CONSOLE_FULL_CONTROL` being enabled.
* `CONSOLE_HISTORY` - Enables history accessible via up/down arrow keys with
the specified command buffer size (default is 0). This depends on
`CONSOLE_FULL_CONTROL` being enabled.

## Tests

The unit tests can be run by running `make` within the `tests` directory.
These tests are built using
[googletest](https://github.com/google/googletest) and depend on the
`gtest` library being available on the system.

## Example

```
#include "anchor/console/console.h"

CONSOLE_COMMAND_DEF(gpio_set, "Sets a GPIO pin",
    CONSOLE_STR_ARG_DEF("port", "The port <A,B,C>"),
    CONSOLE_INT_ARG_DEF("pin", "The pin <0-15>"),
    CONSOLE_INT_ARG_DEF("value", "The value <0-1>")
);

CONSOLE_COMMAND_DEF(gpio_get, "Gets a GPIO pin value",
    CONSOLE_STR_ARG_DEF("port", "The port <A,B,C>"),
    CONSOLE_INT_ARG_DEF("pin", "The pin <0-15>")
);

static void gpio_set_command_handler(void) {
    const char* port;
    int pin, value;
    console_command_get_str_arg("port", &port);
    console_command_get_int_arg("pin", &pin);
    console_command_get_int_arg("value", &value);

    gpio_write(port, pin, value);
}

static void gpio_get_command_handler(void) {
    const char* port;
    int pin;
    console_command_get_str_arg("port", &port);
    console_command_get_int_arg("pin", &pin);

    printf("value=%d", gpio_read(port, pin));
}

void main(void) {
    const console_init_t init_console = {
        .write_function = console_write_function,
    };
    console_init(&init_console);
    console_command_register(gpio_set);
    console_command_register(gpio_get);

    while (true) {
        uint8_t buffer[64];
        const uint32_t len = serial_read(buffer, sizeof(buffer));
        console_process(buffer, len);
        // ...
    }
}
```

```
> help
Available commands:
  help     - List all commands, or give details about a specific command
  gpio_set - Sets a GPIO pin
  gpio_get - Gets a GPIO pin value
> help gpio_get
Gets a GPIO pin value
Usage: gpio_get port pin
  port - The port <A,B,C>
  pin  - The pin <0-15>
> gpio_set A 1 1
> gpio_get A 1
value=1
> gpio_set A 1 0
> gpio_get A 1
value=0
```
