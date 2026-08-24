#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#include <stdint.h>
#include <common_consts.h>

typedef enum {
    UPDATE_SYSTEM_TIME,
    UPDATE_SYSTEM_WEATHER,
    DATED_WEATHER_QUERY,
} ble_req_type_t;


typedef struct ble_req {
    ble_req_type_t req_code;
    uint8_t app_id;
    uint16_t req_data_len;
    uint8_t *req_data;
} ble_req_t;

typedef enum {
    WHATSAPP,
    MESSAGE,
    NAVIGATION,
    CALL,
    UNKNOWN
} phone_app_t;

typedef struct {
    phone_app_t app;
    char app_name[14];
    char body[100];
    char action_name[15];
    char dismiss_text[15];
    void (*action_handler)(void);
    void (*dismiss_handler)(unsigned int);
} notification_t;

typedef enum {
    DATED_WEATHER_REQUEST
} app_ble_req_t;

typedef struct {
    app_ble_req_t req;
    uint8_t req_app;
    uint8_t data[MAX_APP_RESPONSE_SIZE];
} app_update_t;

typedef struct {
    uint8_t day;
    uint8_t month;
    uint16_t year;
    uint8_t hr;
    uint8_t min;
    uint8_t sec;
    uint8_t d_week;
} date_time_t;

typedef struct {
    uint32_t last_sync_time;
    uint32_t time_sync_uptime;
    uint8_t valid;
} time_sync_t;

typedef struct {
    int16_t temperature; // Scaled by 10
    uint8_t humidity;
    uint8_t precip_prob;
    uint8_t weather_code;
    uint8_t wind_speed;
} hourly_weather_t;

typedef struct {
    uint32_t expires_at;
    hourly_weather_t hourly_today[24];
} weather_sync_t;

#endif /* COMMON_TYPES_H */
