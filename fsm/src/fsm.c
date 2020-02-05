#include "anchor/fsm/fsm.h"

#ifdef FSM_USE_LOGGING
#include "anchor/logging/logging.h"
#endif

#include <stdbool.h>

#define GET_IMPL(FSM) ((fsm_impl_t*)((FSM)->_private))

typedef struct {
    fsm_state_t state;
    bool in_transition;
} fsm_impl_t;
_Static_assert(sizeof(fsm_impl_t) == sizeof(((fsm_t*)0)->_private), "Invalid fsm_impl_t size");

void fsm_init(fsm_t* fsm) {
    fsm_impl_t* impl = GET_IMPL(fsm);
    impl->state = fsm->initial_state;
}

void fsm_process_event(fsm_t* fsm, fsm_event_t event) {
    fsm_impl_t* impl = (fsm_impl_t*)(fsm->_private);
    if (impl->in_transition) {
#ifdef FSM_USE_LOGGING
        LOG_ERROR("Attempting to process event from handler, dropping event");
#endif
        return;
    }
    for (uint32_t i = 0; i < fsm->num_transitions; i++) {
        const fsm_transition_t* transition = &fsm->transitions[i];
        if (impl->state == transition->from_state && event == transition->event) {
            // execute this transition
#ifdef FSM_USE_LOGGING
            LOG_INFO("Executing transition from %s to %s (event=%s)", transition->from_state->name, transition->to_state->name, event->name);
#endif
            impl->in_transition = true;
            fsm->on_state_exit_handler(fsm, impl->state);
            impl->state = transition->to_state;
            fsm->on_state_enter_handler(fsm, impl->state);
            impl->in_transition = false;
            return;
        }
    }
}
