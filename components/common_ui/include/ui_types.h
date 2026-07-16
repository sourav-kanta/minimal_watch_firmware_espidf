#ifndef UI_TYPES_H
#define UI_TYPES_H

#include <common_types.h>
#include <lvgl.h>

typedef struct {
    date_time_t time;
    hourly_weather_t weather;
} wf_update_payload_t;

typedef struct {
    uint8_t wf_id;
    uint16_t wf_perms;
    const char *name;
    void (*draw_watchface) (lv_obj_t*);
    void (*update_watchface) (wf_update_payload_t*);
    void (*del_watchface) (void);
} watchface_t;

#endif /* UI_TYPES_H */
