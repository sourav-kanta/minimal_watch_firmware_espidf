#ifndef RUNTIME_UTILS_H
#define RUNTIME_UTILS_H

void init_timer(esp_timer_handle_t *timer, void (*callback_func)(void*), const char* timer_name, void* data);
void safe_timer_cleanup(esp_timer_handle_t *timer_ptr);

#endif /* RUNTIME_UTILS_H */
