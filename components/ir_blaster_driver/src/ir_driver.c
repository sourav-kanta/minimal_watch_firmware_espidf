#include <ir_driver.h>
#include <ir_driver_unsafe.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <esp_system.h>
#include <stdatomic.h>
#include <esp_log.h>
#include <string.h>

#define MS_BETWEEN_IR_BLASTS        100
#define MAX_CONCURRENT_IR_MESSAGES  5
#define IR_TASK_STACK_DEPTH         2048
#define IR_TASK_PRIORITY            1

static QueueHandle_t ir_queue = NULL;
static bool initialized = false;
static atomic_bool abort_flag = false;
static SemaphoreHandle_t exit_sem = NULL;
static const char* TAG = "IR Blaster Driver";
static TaskHandle_t ir_task = NULL;

static void ir_task_fn(void* arg) {
    while(!atomic_load(&abort_flag)) {
        ir_blaster_data_t msg;
        assert(ir_queue);
        if(xQueueReceive(ir_queue, &msg, portMAX_DELAY) != pdTRUE) {
            ESP_LOGE(TAG, "IR queue corrupted");
            continue;
        }
        if(msg.frequency == UINT32_MAX && msg.payload_len == UINT8_MAX && msg.payload_data == NULL) {
            // Received poisoned message, exit loop
            break;
        }
        bool success = ir_blaster_driver_unsafe_send_data(&msg);
        if(!success) {
            ESP_LOGE(TAG, "Failed to send IR packet!");
        }
        if(msg.payload_data) free(msg.payload_data);
        else ESP_LOGE(TAG, "Memory leak in IR payload!");
        vTaskDelay(pdMS_TO_TICKS(MS_BETWEEN_IR_BLASTS));
    }
    assert(exit_sem);
    xSemaphoreGive(exit_sem);
    vTaskDelete(NULL);
} 

bool ir_blaster_driver_send_data(ir_blaster_data_t* data) {
    if(!data || !data->payload_data || !initialized || !ir_queue || atomic_load(&abort_flag)) return false;
    uint8_t* payload_copy = malloc(data->payload_len);
    if(!payload_copy) {
        ESP_LOGE(TAG, "Error deep copying IR payload, check mem!");
        return false;
    }
    ir_blaster_data_t new_msg = *data;
    memcpy(payload_copy, data->payload_data, data->payload_len);
    new_msg.payload_data = payload_copy;
    if(xQueueSend(ir_queue, &new_msg, pdMS_TO_TICKS(0)) != pdTRUE) {
        free(payload_copy);
        return false;
    }
    return true;
}

void ir_blaster_driver_init(void) {
    if(initialized) return;
    ir_blaster_driver_unsafe_init();
    atomic_store(&abort_flag, false);
    ir_queue = xQueueCreate(MAX_CONCURRENT_IR_MESSAGES, sizeof(ir_blaster_data_t));
    if(!ir_queue) {
        esp_system_abort("Unable to allocate IR Queue");
    }
    BaseType_t success = xTaskCreate(ir_task_fn, "IR Task", IR_TASK_STACK_DEPTH, NULL, IR_TASK_PRIORITY, &ir_task);
    if(success != pdPASS) {
        esp_system_abort("Failed to create IR task");
    }
    initialized = true;
}

void ir_blaster_driver_deinit(void) {
    if(!initialized) return;
    initialized = false;
    ir_blaster_data_t poison_msg = { 
        .frequency = UINT32_MAX, 
        .payload_len = UINT8_MAX, 
        .payload_data = NULL 
    };
    exit_sem = xSemaphoreCreateBinary();
    atomic_store(&abort_flag, true);
    if(xQueueSend(ir_queue, &poison_msg, pdMS_TO_TICKS(0)) != pdTRUE) {
        ESP_LOGI(TAG, "Queue is full, should be able to abort anyway!");
    }
    if(xSemaphoreTake(exit_sem, pdMS_TO_TICKS(5*1000)) == pdTRUE) {
        ESP_LOGI(TAG, "Deinitialized IR driver");
    }
    else {
        ESP_LOGE(TAG, "Failed to close IR task(no pending messages), forcing task close");
        if(ir_task) vTaskDelete(ir_task);
    }
    if(ir_queue) {
        ir_blaster_data_t leftover;
        while(xQueueReceive(ir_queue, &leftover, 0) == pdTRUE) {
            if(leftover.payload_data) free(leftover.payload_data);
        }
        vQueueDelete(ir_queue);
        ir_queue = NULL;
    }
    ir_blaster_driver_unsafe_deinit();
    if(exit_sem) { 
        vSemaphoreDelete(exit_sem);
        exit_sem = NULL;
    }
} 
