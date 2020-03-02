#pragma once

#include <inttypes.h>

#ifdef __SDCC_VERSION_MAJOR
#define _FSM_CONTEXT_SIZE (sizeof(uintptr_t) + sizeof(uint8_t))
#else
#define _FSM_CONTEXT_SIZE (sizeof(uintptr_t) * 2)
#endif

typedef struct {
    const char* name;
} fsm_state_impl_t;
typedef const fsm_state_impl_t* fsm_state_t;

// Define a new FSM state with a given name
#define FSM_STATE_DEF(NAME) \
    static const fsm_state_impl_t _##NAME##_fsm_state = { \
        .name = #NAME, \
    }
// Reference a previously-defined FSM state
#define FSM_STATE(NAME) &_##NAME##_fsm_state

typedef struct {
    const char* name;
} fsm_event_impl_t;
typedef const fsm_event_impl_t* fsm_event_t;

// Define a new FSM event with a given name
#define FSM_EVENT_DEF(NAME) \
    static const fsm_event_impl_t _##NAME##_fsm_event = { \
        .name = #NAME, \
    }
// Reference a previously-defined FSM event
#define FSM_EVENT(NAME) &_##NAME##_fsm_event

typedef struct {
    fsm_state_t from_state;
    fsm_state_t to_state;
    fsm_event_t event;
} fsm_transition_t;

// Structure which represents an FSM
typedef struct fsm {
    // Allocated space for private context to be used by the FSM implementation only
    uint8_t _private[_FSM_CONTEXT_SIZE];
    // Supported FSM transitions
    const fsm_transition_t* transitions;
    // Number of transitions
    uint32_t num_transitions;
    // Handler called when a state is entered
    void (*on_state_enter_handler)(const struct fsm* fsm, fsm_state_t state);
    // Handler called when a state is exitted
    void (*on_state_exit_handler)(const struct fsm* fsm, fsm_state_t state);
    // Initial FSM state
    fsm_state_t initial_state;
} fsm_t;

// Initialize the FSM
void fsm_init(fsm_t* fsm);

// Process an FSM event
void fsm_process_event(fsm_t* fsm, fsm_event_t event);
