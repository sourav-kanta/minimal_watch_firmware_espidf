#ifndef EVENT_REGISTRY_TYPES_H
#define EVENT_REGISTRY_TYPES_H

#include <event_manager.h>
#include <event_consts.h>
#include <stdint.h>

typedef struct
{
    event_handler_t handlers[MAX_EVENT_SUBSCRIBERS];
    uint8_t count;
} event_subscription_t;

#endif /* EVENT_REGISTRY_TYPES_H */
