# FSM

A small library which implements an event-driven FSM, where the implementation
exists entirely within state entry and exit handlers.

## Usage

The first step is to define the set of states and events which are involved in
the FSM. This is accomplished using the `FSM_STATE_DEF(NAME)` and
`FSM_EVENT_DEF(NAME)` macros. Next, all valid transitions should be defined in
an array of `fsm_transition_t` instances. The `FSM_STATE(NAME)` and
`FSM_EVENT(NAME)` macros can be used to referenced the defined states and
events respectively. These macros allow for convenient naming and namespacing
of states and events, while allowing the transitions array to be defined at
compile time, and thus live in program space (.rodata), as opposed to RAM.

A state entry and exit handler should be defined, and passed along with the
the transitions array to fsm_init() to initialize the FSM.

Events are then passed to `fsm_process_event()` which generates appropriate
state entry / exit callbacks, if there is an appropriate transition defined.

## Compile Options

Additional features of the FSM library can be enabled by compiling with the
following defines:
* `FSM_USE_LOGGING` - enable logging of state transitions via the `logging`
library.

## Example

```
// FSM States
FSM_STATE_DEF(OFF);
FSM_STATE_DEF(GREEN);
FSM_STATE_DEF(RED);

// FSM Events
FSM_EVENT_DEF(REQUESTED_OFF);
FSM_EVENT_DEF(REQUESTED_GREEN);
FSM_EVENT_DEF(REQUESTED_RED);

// FSM Transitions
static const fsm_transition_t LED_FSM_TRANSITIONS[] = {
    {FSM_STATE(OFF), FSM_STATE(GREEN), FSM_EVENT(REQUESTED_GREEN)},
    {FSM_STATE(OFF), FSM_STATE(RED), FSM_EVENT(REQUESTED_RED)},
    {FSM_STATE(GREEN), FSM_STATE(OFF), FSM_EVENT(REQUESTED_OFF)},
    {FSM_STATE(GREEN), FSM_STATE(RED), FSM_EVENT(REQUESTED_RED)},
    {FSM_STATE(RED), FSM_STATE(OFF), FSM_EVENT(REQUESTED_OFF)},
    {FSM_STATE(RED), FSM_STATE(GREEN), FSM_EVENT(REQUESTED_GREEN)},
};

static void fsm_on_state_enter_handler(const fsm_t* fsm, fsm_state_t state) {
    if (state == FSM_STATE(GREEN)) {
        gpio_set(GREEN_LED_PIN, HIGH);
    } else if (state == FSM_STATE(RED)) {
        gpio_set(RED_LED_PIN, HIGH);
    }
}

static void fsm_on_state_exit_handler(const fsm_t* fsm, fsm_state_t state) {
    if (state == FSM_STATE(GREEN)) {
        gpio_set(GREEN_LED_PIN, LOW);
    } else if (state == FSM_STATE(RED)) {
        gpio_set(RED_LED_PIN, LOW);
    }
}

void led_thread(void) {
    fsm_t fsm = {
        .transitions = FSM_TRANSITIONS,
        .num_transitions = ARRAY_LENGTH(FSM_TRANSITIONS),
        .on_state_enter_handler = fsm_on_state_enter_handler,
        .on_state_exit_handler = fsm_on_state_exit_handler,
        .initial_state = FSM_STATE(OFF),
    };
    fsm_init(&fsm);

    while (true) {
        // get request
        if (/* requested off */) {
            fsm_process_event(&fsm, FSM_EVENT(REQUESTED_OFF));
        } else if (/* requested green */) {
            fsm_process_event(&fsm, FSM_EVENT(REQUESTED_GREEN));
        } else if (/* requested red */) {
            fsm_process_event(&fsm, FSM_EVENT(REQUESTED_RED));
        }
    }
}
```
