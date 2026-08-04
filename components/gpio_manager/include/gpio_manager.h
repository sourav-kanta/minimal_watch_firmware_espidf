#ifndef GPIO_MANAGER_H
#define GPIO_MANAGER_H

void gpio_manager_init(void);
void gpio_manager_deinit(void);
void gpio_manager_debug_led_off(void);
void gpio_manager_debug_led_on(void);
bool gpio_manager_backlight_set_brightness(int percent);
int  gpio_manager_backlight_get_brightness(void);
void gpio_manager_power_backlight(void);
void gpio_manager_backlight_off(void);

#endif /* GPIO_MANAGER_H */
