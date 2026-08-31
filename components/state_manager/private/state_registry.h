#ifndef STATE_REGISTRY_H
#define STATE_REGISTRY_H

#include <common_types.h>

typedef struct {
    uint32_t last_req_time;
    bool request_pending;
} state_entry_t;


typedef struct {
    state_entry_t time;
    state_entry_t weather;
    state_entry_t alarms;
} state_registry_t;

typedef struct {
    time_sync_t time_state;
    weather_sync_t weather_state;
    alarm_sync_t alarms;    
} watch_state_t;

#endif /* STATE_REGISTRY_H */
