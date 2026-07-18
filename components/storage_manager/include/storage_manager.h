#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

typedef void (*retrieve_callback_t) (char* key, uint8_t* dest, uint8_t* len); 

void storage_manager_init(void);
void storage_manager_deinit(void);
bool storage_manager_save_key(uint8_t app_id, const char* key, const uint8_t* data, uint8_t len);
bool storage_manager_retrieve_key(uint8_t app_id, const char* key, uint8_t* dest, uint8_t* len);
bool storage_manager_retrieve_all(uint8_t app_id, retrieve_callback_t callback_func);
bool storage_manager_erase_key(uint8_t app_id, const char* key);
bool storage_manager_erase_all(uint8_t app_id);
unsigned int storage_manager_get_app_storage_size(uint8_t app_id);

#endif /* STORAGE_MANAGER_H */
