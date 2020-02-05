# Anchor

Anchor is a collection of various embedded firmware libraries developed by Skip
Transport, Inc. These libraries were all developed to be easily re-usable and
not dependant on a specific MCU or RTOS.

# Libraries

This repository is organized as a collection of libraries which are largely
independent of each other. You are free to pick and chose the libraries which
are most useful in your application, without needing to include them all. Each
library is organized within a subdirectory, with a README providing information
specific to the library.

In general, including a library is as simple as adding all the `.c` files
within `<lib>/src` to your build system as C sources to be compiled, and adding
`<lib>/include` to your build system's include path.

## Logging

The logging library makes it easy to write out consistent and useful debug
logs. This is often done over a UART connection, but the library is agnostic to
the protocol being used.

```
  0:00:00.626 WARN  system.c:199: Last reset due to software reset
  0:00:00.632 INFO  system.c:243: Serial number is 003b001b524b501020353548
  0:00:00.639 INFO  system.c:257: Detected DVT hardware
  0:00:00.648 INFO  piezo.c:45: is_present=1
  0:00:16.732 ERROR SONAR:link_layer.c:161: Invalid packet: Response sequence number does not match request
```

## FSM

The FSM library is a very lightweight set of macros and functions for defining
event-based finite state machines with enter/exit handlers for each state. See
the README within the `fsm` subdirectory for more information and an example.

## Console

The console library can be used to provide a debug interface to your device.
The library is designed to be highly configurable to allow the user to decide
between more features and code-space / runtime efficiencies.

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

# Contributing

If you find a bug, or have an idea for how we can improve this library, please
let us know by creating an issue. Opening up a pull request is even better!

# License

These libraries are made available under the MIT license. See the LICENSE file
for more information.
