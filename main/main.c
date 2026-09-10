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
#include <wf_manager.h>
#include <ui_base.h>
#include <sleep_lock_service.h>
#include <uart_ota.h>

#include <settings_app.h>
#include <weather_app.h>
#include <alarm_app.h>
#include <stopwatch_app.h>
#include <brickbreaker_game.h>

#include <wf_abstract_dark.h>
#include <wf_analog.h>
#include <wf_retro.h>

void app_main(void)
{
    TaskHandle_t main_taskhandle = xTaskGetCurrentTaskHandle();

    init_locks();
    event_manager_init();
    power_manager_init(main_taskhandle);
    sleep_lock_interface_t sleep_lock_api = {
        .sleep_lock_acquire = power_manager_prevent_sleep,
        .sleep_lock_release = power_manager_allow_sleep,
    };
    sleep_lock_service_init(&sleep_lock_api);

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
    
    /* App registration */
    app_manager_add_app(get_weather_app());
    app_manager_add_app(get_settings_app());
    app_manager_add_app(get_alarm_app());
    app_manager_add_app(get_stopwatch_app());
    app_manager_add_app(get_brickbreaker_game());

    ui_manager_init();
    /* Watchface registration */
    watchface_manager_init();
    watchface_manager_register_wf(get_retro_wf());
    watchface_manager_register_wf(get_analog_wf());
    watchface_manager_register_wf(get_abstract_dark_wf());

    /* Wire up the UI tabs */
    ui_tab_handlers_t notification_tab = {
        .on_draw = notification_manager_draw_notification_page,
        .on_close = notification_manager_delete_ui,
        .on_resume = NULL,
        .on_suspend = NULL
    };

    ui_tab_handlers_t watchface_tab = {
        .on_draw = watchface_manager_start_wf,
        .on_close = watchface_manager_stop_wf,
        .on_resume = watchface_manager_resume,
        .on_suspend = watchface_manager_suspend
    };

    ui_tab_handlers_t app_tab = {
        .on_draw = app_manager_show_app_picker_ui,
        .on_close = app_manager_del_app_picker_ui,
        .on_resume = NULL,
        .on_suspend = NULL
    };
    ui_base_register_tab(NOTIFY, &notification_tab);
    ui_base_register_tab(WATCHFACE, &watchface_tab);
    ui_base_register_tab(APP, &app_tab);

    notification_manager_init();
    tick_manager_init();
    ui_on();
    tick_manager_generate_tick(TICK_WORK);

    // Wait for 5 work ticks to confirm
    vTaskDelay(pdMS_TO_TICKS(5*1000));
    uart_ota_mark_firmware_valid();
    ESP_LOGI("Main", "Marked firmware as valid");

    shutdown_reason_t reason = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    ui_manager_deinit();
    ble_manager_deinit();
    runtime_manager_deinit();
    tick_manager_deinit();
    notification_manager_deinit();
    app_manager_deinit();
    watchface_manager_deinit();
    state_manager_deinit();
    gpio_manager_deinit();
    sensor_manager_deinit();
    storage_manager_deinit();
    sleep_lock_service_deinit();
    
    if(reason != POWER_MANAGER_SHUTDOWN_REASON_DFU) {
        // Needs to keep the power lock alive
        power_manager_deinit();
    }
    
    event_manager_deinit();

    if(reason == POWER_MANAGER_SHUTDOWN_REASON_DEEP_SLEEP) {
        power_manager_engage_deep_sleep();
    }
    else if(reason == POWER_MANAGER_SHUTDOWN_REASON_DFU) {
        uart_ota_start_firmware_update();
    }
}
