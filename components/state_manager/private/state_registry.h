#ifndef STATE_REGISTRY_H
#define STATE_REGISTRY_H

typedef struct {
    uint32_t last_req_time;
    bool request_pending;
} state_entry_t;


typedef struct {
    state_entry_t time;
    state_entry_t weather;
} state_registry_t;

#endif /* STATE_REGISTRY_H */
