#ifndef COMMON_CONSTS_H
#define COMMON_CONSTS_H

#define DISPLAY_LCD_H_RES               128
#define DISPLAY_LCD_V_RES               160
#define DISPLAY_LVGL_CHUNK_HEIGHT       40
#define DISPLAY_LVGL_BUFFER_SIZE        (DISPLAY_LCD_H_RES * DISPLAY_LVGL_CHUNK_HEIGHT * 2)
#define DISPLAY_MIN_USER_INPUT_TIMEOUT  5000
#define DISPLAY_LVGL_STACK_SIZE         8192
#define DISPLAY_LVGL_CPU_CORE           1

#define MAX_WATCHFACES                  10
#define MAX_APPS                        15
#define MAX_SYSTEM_APPS                 5

#define MAX_APP_RESPONSE_SIZE           220
#define MAX_NOTIFICATION_TITLE_SIZE     14
#define MAX_NOTIFICATION_BODY_SIZE      100
#define BT_DEVICE_NAME                  "MINWH"

#define WATCHFACE_SYSTEM_APP_ID         1
#define CORE_SYSTEM_APP_ID              2

#endif /* COMMON_CONSTS_H */
