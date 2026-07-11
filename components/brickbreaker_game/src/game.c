#include <lvgl.h>
#include <stdbool.h>
#include <app_types.h>
#include <global_locks.h>
#include "assets/app_ico.h"
#include <ui_utils.h>

// ==========================================
// 1. GAME DIMENSIONS & PROPERTIES (128x160)
// ==========================================
#define GAME_WIDTH      128
#define GAME_HEIGHT     160

#define PADDLE_WIDTH     24  // Scaled for 128px screen width
#define PADDLE_HEIGHT     6
#define BALL_SIZE         4  // Miniature 4x4 ball

#define BRICK_ROWS        4
#define BRICK_COLS        5  // 5 columns fit nicely across 128px
#define BRICK_WIDTH      20
#define BRICK_HEIGHT      6
#define BRICK_PADDING     3

// ==========================================
// 2. STRUCTURES & GLOBAL STATES
// ==========================================
typedef struct {
    int x, y;
    int w, h;
    bool active;
    lv_obj_t *obj;
} Brick;

static lv_obj_t *game_screen = NULL;
static lv_obj_t *paddle_obj = NULL;
static lv_obj_t *ball_obj = NULL;
static lv_timer_t *game_timer = NULL;

// Object coordinates and speed states
static int paddle_x = (GAME_WIDTH - PADDLE_WIDTH) / 2;
static int paddle_y = GAME_HEIGHT - PADDLE_HEIGHT - 8;

static float ball_x = GAME_WIDTH / 2.0f;
static float ball_y = GAME_HEIGHT - 30.0f;
static float ball_dx = 1.5f;   // Reduced speed vectors to prevent clipping
static float ball_dy = -1.5f;

static Brick bricks[BRICK_ROWS][BRICK_COLS];
static bool key_left_pressed = false;
static bool key_right_pressed = false;

// ==========================================
// 3. INPUT CALLBACK
// ==========================================
static void game_key_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_key_t key = lv_event_get_key(e);
    
    if(key != LV_KEY_ESC) lv_event_stop_bubbling(e);

    if(code == LV_EVENT_KEY) {
        if(key == LV_KEY_LEFT)  key_left_pressed = true;
        if(key == LV_KEY_RIGHT) key_right_pressed = true;
    }
}

// ==========================================
// 4. CORE TICK LOOP (PHYSICS & STATE UPDATES)
// ==========================================
static void game_tick_timer_cb(lv_timer_t * timer) {
    // A. Move Paddle
    if(key_left_pressed)  paddle_x -= 3;
    if(key_right_pressed) paddle_x += 3;

    // Reset input registers for this frame loop
    key_left_pressed = false;
    key_right_pressed = false;

    // Clamp paddle within game borders
    if(paddle_x < 0) paddle_x = 0;
    if(paddle_x > GAME_WIDTH - PADDLE_WIDTH) paddle_x = GAME_WIDTH - PADDLE_WIDTH;

    // B. Move Ball
    ball_x += ball_dx;
    ball_y += ball_dy;

    // C. Wall Collision Checks (Left, Right, Top)
    if(ball_x <= 0 || ball_x >= GAME_WIDTH - BALL_SIZE) ball_dx *= -1.0f;
    if(ball_y <= 0) ball_dy *= -1.0f;

    // Out of bounds / Lose Condition (Bottom Edge)
    if(ball_y >= GAME_HEIGHT) {
        ball_x = GAME_WIDTH / 2.0f;
        ball_y = GAME_HEIGHT - 30.0f;
        ball_dy = -1.5f; // Relaunch upward
    }

    // D. Paddle Collision Check
    if(ball_y + BALL_SIZE >= paddle_y && ball_y <= paddle_y + PADDLE_HEIGHT) {
        if(ball_x + BALL_SIZE >= paddle_x && ball_x <= paddle_x + PADDLE_WIDTH) {
            ball_dy *= -1.0f;
            ball_y = paddle_y - BALL_SIZE - 1; // Safely shift ball outside the paddle
        }
    }

    // E. Brick Grid Collision Check
    for(int r = 0; r < BRICK_ROWS; r++) {
        for(int c = 0; c < BRICK_COLS; c++) {
            if(!bricks[r][c].active) continue;

            Brick *b = &bricks[r][c];
            if(ball_x + BALL_SIZE >= b->x && ball_x <= b->x + b->w &&
               ball_y + BALL_SIZE >= b->y && ball_y <= b->y + b->h) {

                b->active = false;
                WITH_UI_LOCK() {
                    lv_obj_add_flag(b->obj, LV_OBJ_FLAG_HIDDEN); // Tells LVGL renderer to skip this widget
                }
                ball_dy *= -1.0f;
                break;
            }
        }
    }

    // F. Synchronize Engine States to LVGL Canvas Layer
    WITH_UI_LOCK() {
        lv_obj_set_pos(paddle_obj, paddle_x, paddle_y);
        lv_obj_set_pos(ball_obj, (int)ball_x, (int)ball_y);
    }
}

// ==========================================
// 5. INITIALIZATION & SCENE UI BUILDING
// ==========================================
void init_brick_breaker(lv_obj_t* parent) {
    // A. Base Container Display Area Setup
    WITH_UI_LOCK() {
        game_screen = lv_obj_create(parent);
        lv_obj_set_size(game_screen, GAME_WIDTH, GAME_HEIGHT);
        lv_obj_center(game_screen);
        lv_obj_set_style_bg_color(game_screen, lv_color_black(), 0);
        lv_obj_set_style_border_width(game_screen, 0, 0);
        lv_obj_set_style_pad_all(game_screen, 0, 0);
        lv_obj_remove_flag(game_screen, LV_OBJ_FLAG_SCROLLABLE);

        // B. Instantiate Player Paddle Widget
        paddle_obj = lv_obj_create(game_screen);
        lv_obj_set_size(paddle_obj, PADDLE_WIDTH, PADDLE_HEIGHT);
        lv_obj_set_style_bg_color(paddle_obj, lv_color_hex(0x00FF00), 0); // Retro Green
        lv_obj_set_style_border_width(paddle_obj, 0, 0);
        lv_obj_set_pos(paddle_obj, paddle_x, paddle_y);

        // C. Instantiate Ball Widget
        ball_obj = lv_obj_create(game_screen);
        lv_obj_set_size(ball_obj, BALL_SIZE, BALL_SIZE);
        lv_obj_set_style_bg_color(ball_obj, lv_color_hex(0xFFFFFF), 0);   // White
        lv_obj_set_style_radius(ball_obj, LV_RADIUS_CIRCLE, 0);           // Round profile
        lv_obj_set_style_border_width(ball_obj, 0, 0);
        lv_obj_set_pos(ball_obj, (int)ball_x, (int)ball_y);

        // D. Generate Centered Brick Matrix
        int start_x = (GAME_WIDTH - ((BRICK_COLS * (BRICK_WIDTH + BRICK_PADDING)) - BRICK_PADDING)) / 2;
        int start_y = 15;

        for(int r = 0; r < BRICK_ROWS; r++) {
            for(int c = 0; c < BRICK_COLS; c++) {
                bricks[r][c].x = start_x + c * (BRICK_WIDTH + BRICK_PADDING);
                bricks[r][c].y = start_y + r * (BRICK_HEIGHT + BRICK_PADDING);
                bricks[r][c].w = BRICK_WIDTH;
                bricks[r][c].h = BRICK_HEIGHT;
                bricks[r][c].active = true;

                bricks[r][c].obj = lv_obj_create(game_screen);
                lv_obj_set_size(bricks[r][c].obj, BRICK_WIDTH, BRICK_HEIGHT);
                lv_obj_set_pos(bricks[r][c].obj, bricks[r][c].x, bricks[r][c].y);

                // Layout coloring (Alternates red and orange blocks)
                uint32_t color = (r % 2 == 0) ? 0xFF0000 : 0xFFAA00;
                lv_obj_set_style_bg_color(bricks[r][c].obj, lv_color_hex(color), 0);
                lv_obj_set_style_border_width(bricks[r][c].obj, 0, 0);
            }
        }

        lv_obj_add_event_cb(game_screen, game_key_event_cb, LV_EVENT_KEY, NULL);

        make_obj_navigable(game_screen);
        lv_group_focus_obj(game_screen);

        game_timer = lv_timer_create(game_tick_timer_cb, 30, NULL);
    }
}

void close_game(void) {
    WITH_UI_LOCK() {
        lv_timer_delete(game_timer);
        lv_obj_delete(game_screen);
    }
    game_screen = NULL;
}


static application_t game = {
    .name = "Smashball",
    .ico = &app_ico,
    .draw_app = init_brick_breaker,
    .close_app = close_game
};

application_t* get_brickbreaker_game() {
    return &game;
}
