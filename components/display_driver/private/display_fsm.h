#ifndef DISPLAY_FSM_H
#define DISPLAY_FSM_H

#include <display_driver.h>

void init_fsm();
bool fsm_display_state_transition(display_state_t);
display_state_t fsm_get_current_display_state();

#endif /* DISPLAY_FSM_H */
