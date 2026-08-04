#include <app_utils.h>
#include <app_types.h>
#include <common_consts.h>
#include <state_manager.h>
#include <common_types.h>
#include <event_manager.h>
#include <time.h>
#include <string.h>

uint8_t get_system_app_id(const application_t* app) {
    return MAX_SYSTEM_APPS + MAX_WATCHFACES + app->app_id - 1; 
}

bool check_app_permission(const application_t* app, app_perm_t perm) {
    assert(app);
    if(!app) return false;
    return (app->app_perms & perm) == perm;
}
