#ifndef UI_BASE_TYPES_H
#define UI_BASE_TYPES_H

#include <common_types.h>
#include <lvgl.h>

typedef enum {
    BASE_SCREEN_EVENT_ALARM,
    BASE_SCREEN_EVENT_CALL,
    BASE_SCREEN_EVENT_NOTIFICATION
} ui_base_screen_event_type_t;

typedef struct {
    ui_base_screen_event_type_t event_type;
    union Data {
       alarm_t alarm_data;
       notification_t notification_data;
    } data;
} ui_base_screen_event_t; 

typedef void (*ui_draw_cb_t)(lv_obj_t* parent);
typedef void (*ui_action_cb_t)(void);

typedef struct {
    ui_draw_cb_t   on_draw;
    ui_action_cb_t on_close;
    ui_action_cb_t on_suspend;
    ui_action_cb_t on_resume;
} ui_tab_handlers_t;

typedef enum UI_STATE {
    NOTIFY,
    WATCHFACE,
    APP,
    UISTATE_INVALID
} ui_state_t;

#endif /* UI_BASE_TYPES_H */
