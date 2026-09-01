#ifndef UI_BASE_TYPES_H
#define UI_BASE_TYPES_H

#include <common_types.h>

typedef enum {
    BASE_SCREEN_EVENT_ALARM,
    BASE_SCREEN_EVENT_CALL,
    BASE_SCREEN_EVENT_NOTIFICATION
} ui_base_screen_event_type_t;


typedef void (*on_finish_event_callback_t)(void);
typedef struct {
    ui_base_screen_event_type_t event_type;
    union Data {
       alarm_t alarm_data;
       notification_t notification_data;
    } data;
} ui_base_screen_event_t; 

#endif /* UI_BASE_TYPES_H */
