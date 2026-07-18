#include <storage_manager.h>
#include <storage_consts.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_assert.h>
#include <nvs_flash.h>
#include <string.h>
#include <runtime_manager.h>

static const char *TAG = "STORAGE_MANAGER";
static bool initialized = false;
static const char* STORAGE_PARTITION_LABEL = "storage";

// Operation types for the background worker
typedef enum {
    STORAGE_OP_SAVE,
    STORAGE_OP_ERASE_KEY,
    STORAGE_OP_ERASE_ALL
} storage_op_t;

typedef struct {
    uint8_t app_id;
    storage_op_t op_type;
    uint8_t data_len; 
    char key[16];
    uint8_t data[STORAGE_MAX_DATA_SIZE];
} storage_work_t;
ESP_STATIC_ASSERT(sizeof(storage_work_t) <= MAX_WORKER_ARG_PAYLOAD, "Work struct overflow");

void storage_manager_init(void) {
    if(initialized) return;
    esp_err_t err = nvs_flash_init_partition(STORAGE_PARTITION_LABEL);
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        if (nvs_flash_erase_partition(STORAGE_PARTITION_LABEL) == ESP_OK) {
            err = nvs_flash_init_partition(STORAGE_PARTITION_LABEL);
        }
    }

    if (err == ESP_OK) {
        initialized = true;
        ESP_LOGI(TAG, "Storage manager initialized successfully.");
    } else {
        ESP_LOGE(TAG, "Failed to initialize flash storage: %s", esp_err_to_name(err));
    }
}

void storage_manager_deinit(void) {
    if (!initialized) return;
    nvs_flash_deinit_partition(STORAGE_PARTITION_LABEL);
    initialized = false;
    ESP_LOGI(TAG, "Storage manager deinitialized.");
}

static void storage_manager_execute_work(void *arg, runtime_abort_flag_t* flag) {
    storage_work_t* work = (storage_work_t*) arg;
    if (!work) return;

    char ns_name[16];
    snprintf(ns_name, sizeof(ns_name), "app_%03u", work->app_id);

    nvs_handle_t handle;
    if (nvs_open_from_partition(STORAGE_PARTITION_LABEL, ns_name, NVS_READWRITE, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "Could not open namespace %s for operation %d", ns_name, work->op_type); 
        return;
    }

    esp_err_t err = ESP_FAIL;

    switch (work->op_type) {
        case STORAGE_OP_SAVE:
            if (strnlen(work->key, 16) <= 15) {
                err = nvs_set_blob(handle, work->key, work->data, work->data_len);
                if (err == ESP_OK) {
                    err = nvs_commit(handle);
                    ESP_LOGD(TAG, "Write successful for key %s, len: %u", work->key, work->data_len);
                }
            }
            break;

        case STORAGE_OP_ERASE_KEY:
            if (strnlen(work->key, 16) <= 15) {
                err = nvs_erase_key(handle, work->key);
                if (err == ESP_OK) {
                    err = nvs_commit(handle);
                    ESP_LOGD(TAG, "Async erase successful for key: %s", work->key);
                } else if (err == ESP_ERR_NVS_NOT_FOUND) {
                    ESP_LOGW(TAG, "Key %s not found during async erase", work->key);
                }
            }
            break;

        case STORAGE_OP_ERASE_ALL:
            err = nvs_erase_all(handle);
            if (err == ESP_OK) {
                err = nvs_commit(handle);
                ESP_LOGI(TAG, "Async clear complete for namespace: %s", ns_name);
            }
            break;
    }

    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Storage operation failed: %s", esp_err_to_name(err));
    }

    nvs_close(handle);
}

bool storage_manager_save_key(uint8_t app_id, const char* key, const uint8_t* data, uint8_t len) {
    if(!initialized) return false;
    if (!key || !data || (strnlen(key, 16) > 15) || len > STORAGE_MAX_DATA_SIZE) {
        ESP_LOGE(TAG, "Invalid write parameters");
        return false;
    }
    
    storage_work_t work = {
        .app_id = app_id,
        .op_type = STORAGE_OP_SAVE,
        .data_len = len
    };
    memcpy(work.data, data, len); 
    strncpy(work.key, key, sizeof(work.key) - 1);
    
    runtime_work_item_t delayed_work = {
        .handler = storage_manager_execute_work,
        .type = WORK_TYPE_SYSTEM,
    };
    memcpy(delayed_work.arg_payload, &work, sizeof(work));
    return schedule_system_work(&delayed_work);
}

bool storage_manager_erase_key(uint8_t app_id, const char* key) {
    if (!initialized) return false;
    if (!key || (strnlen(key, 16) > 15)) {
        ESP_LOGE(TAG, "Invalid erase key request");
        return false;
    }

    storage_work_t work = {
        .app_id = app_id,
        .op_type = STORAGE_OP_ERASE_KEY
    };
    strncpy(work.key, key, sizeof(work.key) - 1);

    runtime_work_item_t delayed_work = {
        .handler = storage_manager_execute_work,
        .type = WORK_TYPE_SYSTEM,
    };
    memcpy(delayed_work.arg_payload, &work, sizeof(work));
    return schedule_system_work(&delayed_work);
}

bool storage_manager_erase_all(uint8_t app_id) {
    if (!initialized) return false;

    storage_work_t work = {
        .app_id = app_id,
        .op_type = STORAGE_OP_ERASE_ALL
    };

    runtime_work_item_t delayed_work = {
        .handler = storage_manager_execute_work,
        .type = WORK_TYPE_SYSTEM,
    };
    memcpy(delayed_work.arg_payload, &work, sizeof(work));
    return schedule_system_work(&delayed_work);
}

bool storage_manager_retrieve_key(uint8_t app_id, const char* key, uint8_t* dest, uint8_t* len) {
    if(!initialized) return false;
    if (!key || !dest || !len || strnlen(key, 16) > 15 || (*len > STORAGE_MAX_DATA_SIZE)) {
        ESP_LOGE(TAG, "Invalid read request");
        return false;
    }

    char ns_name[16];
    snprintf(ns_name, sizeof(ns_name), "app_%03u", app_id);

    nvs_handle_t handle;
    if (nvs_open_from_partition(STORAGE_PARTITION_LABEL, ns_name, NVS_READONLY, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "Invalid namespace");
        return false;
    }

    size_t required_size = *len;
    esp_err_t err = nvs_get_blob(handle, key, dest, &required_size);
    if (err == ESP_OK) {
        *len = (uint8_t)required_size;
    }
    else {
        ESP_LOGE(TAG, "Error reading key : %s", key);
        nvs_close(handle);
        return false; 
    }

    nvs_close(handle);
    return true;
}

bool storage_manager_retrieve_all(uint8_t app_id, retrieve_callback_t callback_func) {
    if(!initialized) return false;
    if (!callback_func) {
        ESP_LOGE(TAG, "Invalid callback function");
        return false;
    }

    char ns_name[16];
    snprintf(ns_name, sizeof(ns_name), "app_%03u", app_id);

    nvs_iterator_t it = NULL;
    esp_err_t res = nvs_entry_find(STORAGE_PARTITION_LABEL, ns_name, NVS_TYPE_BLOB, &it);
    if (res != ESP_OK) {
        ESP_LOGW(TAG, "No entries found for namespace : %s", ns_name);
        return false;
    }

    nvs_handle_t handle;
    if (nvs_open_from_partition(STORAGE_PARTITION_LABEL, ns_name, NVS_READONLY, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "Error opening namespace : %s", ns_name);
        nvs_release_iterator(it);
        return false;
    }

    while (res == ESP_OK) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);

        uint8_t blob_buffer[STORAGE_MAX_DATA_SIZE];
        size_t entry_size = sizeof(blob_buffer);

        if (nvs_get_blob(handle, info.key, blob_buffer, &entry_size) == ESP_OK) {
            uint8_t final_len = (uint8_t)entry_size;
            callback_func((char*)info.key, blob_buffer, &final_len);
        }

        res = nvs_entry_next(&it);
    }

    nvs_close(handle);
    if (it != NULL) {
        nvs_release_iterator(it);
    }
    return true;
}

unsigned int storage_manager_get_app_storage_size(uint8_t app_id) {
    if (!initialized) return 0;

    char ns_name[16];
    snprintf(ns_name, sizeof(ns_name), "app_%03u", app_id);

    nvs_iterator_t it = NULL;
    esp_err_t res = nvs_entry_find(STORAGE_PARTITION_LABEL, ns_name, NVS_TYPE_BLOB, &it);
    if (res != ESP_OK) {
        return 0; 
    }

    nvs_handle_t handle;
    if (nvs_open_from_partition(STORAGE_PARTITION_LABEL, ns_name, NVS_READONLY, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "Error opening namespace for size calculation: %s", ns_name);
        nvs_release_iterator(it);
        return 0;
    }

    unsigned int total_size = 0;
    while (res == ESP_OK) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);

        size_t entry_size = 0;
        if (nvs_get_blob(handle, info.key, NULL, &entry_size) == ESP_OK) {
            total_size += entry_size;
        }

        res = nvs_entry_next(&it);
    }

    nvs_close(handle);
    if (it != NULL) {
        nvs_release_iterator(it);
    }
    return total_size;
}
