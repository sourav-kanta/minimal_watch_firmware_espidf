#ifndef EVENT_MANAGER_H
#define EVENT_MANAGER_H

#include <common_types.h>

typedef enum {
    EVENT_UI_INACTIVE,
    EVENT_WATCHFACE_UPDATE,
    EVENT_WORK_TICK,
    EVENT_APP_WORK_SCHEDULE,
    EVENT_NOTIFICATION_RECEIVED,
    EVENT_CALL_RECEIVED,
    EVENT_ALARM_TRIGGERED,
    EVENT_BLE_REQUEST,
    EVENT_TIME_SYNC,
    EVENT_WEATHER_SYNC,
    EVENT_COUNT
} event_id_t;

typedef struct {
    event_id_t ev;
    uint32_t payload_len;
    void* data;
} event_t;

typedef void (*event_handler_t)(const event_t *event);

bool event_manager_init(void);
void event_manager_deinit(void);

bool event_subscribe(event_id_t event_id, event_handler_t handler);
bool event_unsubscribe(event_id_t event_id, event_handler_t handler);

bool event_publish(const event_t *event);

#endif /* EVENT_MANAGER_H */
