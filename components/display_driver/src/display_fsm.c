#include <common_types.h>
#include <display_driver.h>
#include <stddef.h>
#include <stdbool.h>
#include <esp_log.h>

static const char *TAG = "DISPLAY_FSM";

static struct {
    display_state_t display_state;
} current_fsm_state;

void init_fsm(void) {
    current_fsm_state.display_state = DISPLAY_STATE_OFF;
}

static void fsm_update_display_state(display_state_t next_state) {
    current_fsm_state.display_state = next_state;
}

bool fsm_display_state_transition(display_state_t next_state) {
    if (current_fsm_state.display_state == next_state) return false;
    
    switch(next_state) {
        case DISPLAY_STATE_OFF :
            fsm_update_display_state(next_state);
            ESP_LOGI(TAG, "Transition to OFF");
            break;
        case DISPLAY_STATE_ON :
            fsm_update_display_state(next_state);
            ESP_LOGI(TAG, "Transition to ON");
            break;
        case DISPLAY_STATE_SLEEP :
            if(current_fsm_state.display_state == DISPLAY_STATE_OFF) {
                ESP_LOGE(TAG, "Invalid transition from OFF to SLEEP");
                return false;
            }
            fsm_update_display_state(next_state);
            ESP_LOGI(TAG, "Transition to SLEEP");
            break;
        default :
            ESP_LOGE(TAG, "Invalid transition, non existant state");
            return false;
    }
    return true;
}

display_state_t fsm_get_current_display_state() {
    return current_fsm_state.display_state;
}

