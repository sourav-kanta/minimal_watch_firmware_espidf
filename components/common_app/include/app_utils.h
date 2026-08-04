#ifndef APP_UTILS_H
#define APP_UTILS_H

#include <app_types.h>
#include <stdint.h>
#include <common_types.h>

uint8_t get_system_app_id(const application_t*); 
bool check_app_permission(const application_t* app, app_perm_t perm);

#endif /* APP_UTILS_H */
