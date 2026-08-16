#ifndef GPIO_MANAGER_H
#define GPIO_MANAGER_H

void gpio_manager_init(void);
void gpio_manager_deinit(void);
bool gpio_manager_backlight_set_brightness(int percent);
int  gpio_manager_backlight_get_brightness(void);
void gpio_manager_power_backlight(void);
void gpio_manager_backlight_off(void);

void gpio_manager_enter_active_mode(void);
void gpio_manager_enter_background_mode(void);

#endif /* GPIO_MANAGER_H */
