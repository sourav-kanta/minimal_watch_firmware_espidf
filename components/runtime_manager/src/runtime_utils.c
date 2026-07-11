#include <esp_log.h>
#include <esp_timer.h>

static const char* TAG = "Runtime Utils";

void init_timer(esp_timer_handle_t *timer, void (*callback_func)(void*), 
                const char* timer_name, void* data) {
    if(*timer) {
        ESP_LOGE(TAG, "Timer %s already exits, skipping", timer_name);
        return;
    }
    esp_timer_create_args_t timer_arg = {
        .callback = callback_func,
        .name = timer_name,
        .arg = data,
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_arg, timer));
}

void safe_timer_cleanup(esp_timer_handle_t *timer_ptr) {
    if (timer_ptr == NULL || *timer_ptr == NULL) {
        ESP_LOGW(TAG, "Timer is already NULL. Skipping cleanup.");
        return;
    }
    if (esp_timer_is_active(*timer_ptr)) {
        esp_timer_stop(*timer_ptr);
    }
    
    if (esp_timer_delete(*timer_ptr) == ESP_OK) {
        *timer_ptr = NULL;
    }
    else {
        ESP_LOGE(TAG, "Timer deletion unsuccessful, possible memory leak");
        *timer_ptr = NULL;
    }
}
