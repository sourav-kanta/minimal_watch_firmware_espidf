#include <stdio.h>
#include <global_locks.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <ui_manager.h>
#include <ble_manager.h>
#include <event_manager.h>
#include <state_manager.h>
#include <runtime_manager.h>
#include <app_manager.h>
#include <nvs_flash.h>
#include <tick_manager.h>
#include <power_manager.h>
#include <gpio_manager.h>
#include <notification_manager.h>
#include <storage_manager.h>
#include <esp_log.h>
#include <sensor_manager.h>

void app_main(void)
{
    init_locks();
    event_manager_init();
    runtime_manager_init();
    storage_manager_init();    
    sensor_manager_init();
    gpio_manager_init();
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(err);
    }
    ble_manager_init();
    state_manager_init();
    app_manager_init();
    ui_manager_init();
    tick_manager_init();
    ui_on();
    power_manager_init();
    notification_manager_init();

    while(1) {
        vTaskDelay(portMAX_DELAY);
    }
}
