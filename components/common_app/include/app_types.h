#ifndef APP_TYPES_H
#define APP_TYPES_H

#include <lvgl.h>
#include <common_types.h>

typedef uint16_t app_perm_t;

#define APP_PERM_ALL            ((app_perm_t) 0x7FFF)
#define APP_FLAG_SYSTEM         ((app_perm_t) 0x8000)
#define APP_PERM_SYSTEM         (APP_FLAG_SYSTEM | APP_PERM_ALL)
#define APP_PERM_BLE            ((app_perm_t) 0x0001)
#define APP_PERM_SENSOR         ((app_perm_t) 0x0002)
#define APP_PERM_WAKELOCK       ((app_perm_t) 0x0004)
#define APP_PERM_VIBRATION      ((app_perm_t) 0x0008)

typedef struct {
    uint8_t app_id;
    app_perm_t app_perms;
    const char *name;
    const lv_image_dsc_t *ico;
    void (*draw_app) (lv_obj_t *);
    void (*close_app) (void);
    void (*refresh_app) (void);
    void (*handle_event) (const app_update_t*);
} application_t;

#endif /* APP_TYPES_H */
