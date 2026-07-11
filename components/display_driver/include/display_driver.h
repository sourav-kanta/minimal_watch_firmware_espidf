#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

typedef enum {
    DISPLAY_STATE_OFF,
    DISPLAY_STATE_ON,
    DISPLAY_STATE_SLEEP,
} display_state_t;

typedef void (*flush_cb_t)(void);

void init_display(flush_cb_t);
void deinit_display(void);
bool display_sleep(void);
bool display_on(void);
bool display_off(void);
void display_draw_bitmap(int x1, int y1, int x2, int y2, const void* pixels);

#endif /* DISPLAY_DRIVER_H */
