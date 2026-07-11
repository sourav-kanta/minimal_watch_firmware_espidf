#ifndef EVENT_MANAGER_H
#define EVENT_MANAGER_H

#include <common_types.h>

typedef void (*event_handler_t)(const event_t *event);

bool event_manager_init(void);
void event_manager_deinit(void);

bool event_subscribe(event_id_t event_id, event_handler_t handler);
bool event_unsubscribe(event_id_t event_id, event_handler_t handler);

bool event_publish(const event_t *event);

#endif /* EVENT_MANAGER_H */
