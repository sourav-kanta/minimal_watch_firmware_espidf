#ifndef APP_MANAGER_PRIVATE_H
#define APP_MANAGER_PRIVATE_H

void open_app(const application_t*, lv_obj_t* parent);
void close_curr_app(void);
bool check_if_app_running(void);
const application_t* get_current_app(void);

#endif /* APP_MANAGER_PRIVATE_H */
