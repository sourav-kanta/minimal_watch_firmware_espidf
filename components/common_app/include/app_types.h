#ifndef APP_TYPES_H
#define APP_TYPES_H

#include <lvgl.h>
#include <common_types.h>

typedef struct {
    uint8_t app_id;
    uint16_t events_perms;
    const char *name;
    const lv_image_dsc_t *ico;
    void (*draw_app) (lv_obj_t *);
    void (*close_app) (void);
    void (*refresh_app) (void);
    void (*handle_event) (const app_update_t*);
} application_t;

#endif /* APP_TYPES_H */
