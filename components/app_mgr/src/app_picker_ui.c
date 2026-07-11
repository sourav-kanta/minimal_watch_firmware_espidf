#include <lvgl.h>
#include <app_types.h>
#include <esp_log.h>
#include <ui_theme.h>
#include <ui_utils.h>
#include <app_manager.h>
#include <common_consts.h>
#include <global_locks.h>

static const int GRID_COLUMNS_PER_ROW = 2;
static const int GRID_ROW_HEIGHT_PX   = 80;

static const char* TAG = "Picker Ui";
static lv_coord_t col_dsc[] = {LV_GRID_FR(1),
                               LV_GRID_FR(1),
                               LV_GRID_TEMPLATE_LAST};
static lv_coord_t *row_dsc;

static const int UI_ICON_DIMENSION_PX   = 32;
static const int UI_LABEL_WIDTH_PX      = 60;
static const int ANIMATION_SPEED_MS       = 250;

static const int STYLE_BORDER_WIDTH_DEFAULT = 0;
static const int STYLE_BORDER_WIDTH_CELL    = 1;
static const int STYLE_BORDER_WIDTH_FOCUSED = 2;
static const int STYLE_PADDING_NONE         = 0;
static const int STYLE_OUTLINE_WIDTH_NONE   = 0;
static lv_obj_t* app_scr = NULL;
static lv_obj_t* app_root_cont = NULL;

static void close_current_app_cb(lv_event_t* e) {
    if(!check_if_app_running()) return;
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_KEY && lv_event_get_key(e) == LV_KEY_ESC) {
        if(lv_group_get_focused(lv_group_get_default()) == app_scr) {
            close_curr_app();
            WITH_UI_LOCK() {
                lv_obj_delete(app_scr);
                lv_event_stop_bubbling(e);
            }
            app_scr = NULL;
        }
    }
}

static void  open_app_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(app_root_cont == NULL || app_scr != NULL) {
        ESP_LOGE(TAG, "App screen is invalid");
        return;
    }
    if(code == LV_EVENT_KEY && lv_event_get_key(e) == LV_KEY_ENTER) {
        application_t* app = (application_t*)lv_event_get_user_data(e);
        if(!app) return;
        WITH_UI_LOCK() {
            app_scr = lv_obj_create(app_root_cont);
            remove_shadow_and_outline(app_scr);
            make_obj_navigable(app_scr);
            lv_group_focus_obj(app_scr);
            lv_obj_add_flag(app_scr, LV_OBJ_FLAG_EVENT_BUBBLE);
            lv_obj_set_size(app_scr, lv_pct(100), lv_pct(100));
            lv_obj_add_event_cb(app_scr, close_current_app_cb,
                                LV_EVENT_KEY,NULL);
            lv_obj_move_foreground(app_scr);
            open_app(app, app_scr);
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, app_scr);
            lv_anim_set_time(&a, ANIMATION_SPEED_MS);
            lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
            lv_anim_set_values(&a, DISPLAY_LCD_V_RES, 0);
            lv_anim_start(&a);
        }
    }
}


static void stop_bubble_cb(lv_event_t * e) {
    uint32_t key = lv_event_get_key(e);

    if(key == LV_KEY_LEFT  || key == LV_KEY_RIGHT ||
       key == LV_KEY_UP    || key == LV_KEY_DOWN  ||
       key == LV_KEY_ENTER)
    {
        lv_event_stop_bubbling(e);
    }
}

void draw_app_picker_ui(lv_obj_t* root, application_t** apps, uint8_t num_apps) {
    int total_rows = (num_apps + 1) / GRID_COLUMNS_PER_ROW;
    row_dsc = lv_malloc(sizeof(lv_coord_t) * (total_rows + 1));
    for(int i = 0; i < total_rows; i++) { row_dsc[i] = GRID_ROW_HEIGHT_PX; }
    row_dsc[total_rows] = LV_GRID_TEMPLATE_LAST;

    WITH_UI_LOCK() {
        app_root_cont = lv_obj_create(root);
        remove_shadow_and_outline(app_root_cont);
        lv_obj_add_flag(app_root_cont, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_set_size(app_root_cont, DISPLAY_LCD_H_RES, DISPLAY_LCD_V_RES);
        lv_obj_set_style_bg_color(app_root_cont, COLOR_THEME_SECONDARY, 0);
        lv_obj_set_style_bg_opa(app_root_cont, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(app_root_cont, STYLE_BORDER_WIDTH_DEFAULT, 0);
        lv_obj_set_style_pad_all(app_root_cont, STYLE_PADDING_NONE, 0);
        lv_obj_set_scrollbar_mode(app_root_cont, LV_SCROLLBAR_MODE_OFF);

        lv_obj_t *cont = lv_obj_create(app_root_cont);
        lv_obj_set_size(cont, DISPLAY_LCD_H_RES, DISPLAY_LCD_V_RES);
        lv_obj_set_layout(cont, LV_LAYOUT_GRID);
        lv_obj_set_style_grid_column_dsc_array(cont, col_dsc, 0);
        lv_obj_set_style_grid_row_dsc_array(cont, row_dsc, 0);
        lv_obj_set_style_bg_opa(cont, STYLE_PADDING_NONE, 0); 
        lv_obj_set_style_border_width(cont, STYLE_BORDER_WIDTH_DEFAULT, 0);
        lv_obj_set_style_pad_all(cont, STYLE_PADDING_NONE, 0);
        lv_obj_set_style_pad_column(cont, STYLE_PADDING_NONE, 0);
        lv_obj_set_style_pad_row(cont, STYLE_PADDING_NONE, 0);
        
        lv_obj_set_style_outline_width(cont, STYLE_OUTLINE_WIDTH_NONE, LV_STATE_ANY);
        lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_OFF);

        lv_obj_add_event_cb(cont, stop_bubble_cb, LV_EVENT_KEY, NULL);
        make_obj_navigable(cont);
        lv_group_focus_obj(app_root_cont);
        lv_gridnav_add(cont, LV_GRIDNAV_CTRL_ROLLOVER); 

        for(int i = 0; i < num_apps; i++) {
            uint8_t col = i % GRID_COLUMNS_PER_ROW;
            uint8_t row = i / GRID_COLUMNS_PER_ROW;

            lv_obj_t* btn = lv_obj_create(cont);
            lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, col, 1, 
                    LV_GRID_ALIGN_STRETCH, row, 1);
            lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(btn, open_app_cb, LV_EVENT_KEY, apps[i]);
            
            lv_obj_set_style_bg_color(btn, COLOR_THEME_SECONDARY, 0);
            lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(btn, STYLE_PADDING_NONE, 0); 
            lv_obj_set_style_border_width(btn, STYLE_BORDER_WIDTH_CELL, 0);
            lv_obj_set_style_border_color(btn, COLOR_THEME_PRIMARY, 0); 
            lv_obj_set_style_pad_all(btn, STYLE_PADDING_NONE, 0);
            lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, 
                    LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

            lv_obj_set_style_outline_width(btn, STYLE_OUTLINE_WIDTH_NONE, LV_STATE_ANY);
            lv_obj_set_style_bg_color(btn, COLOR_THEME_FOCUS_BG, LV_STATE_FOCUSED);
            lv_obj_set_style_border_width(btn, STYLE_BORDER_WIDTH_FOCUSED, LV_STATE_FOCUSED);
            lv_obj_set_style_border_color(btn, COLOR_THEME_ACCENT, LV_STATE_FOCUSED);
            lv_obj_set_style_border_side(btn, LV_BORDER_SIDE_FULL, LV_STATE_FOCUSED);

            lv_obj_t *icon = lv_image_create(btn);
            lv_image_set_src(icon, apps[i]->ico);
            lv_obj_set_size(icon, UI_ICON_DIMENSION_PX, UI_ICON_DIMENSION_PX);
            
            lv_obj_t *label = lv_label_create(btn);
            lv_label_set_text(label, apps[i]->name);
            lv_obj_set_style_text_font(label, &lv_font_montserrat_10, 0);
            lv_obj_set_style_text_color(label, COLOR_THEME_TEXT_PRIMARY, 0);
            lv_obj_set_width(label, UI_LABEL_WIDTH_PX);
            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        }
        lv_obj_update_layout(cont);
    }
}

void del_app_picker_ui(void) {
    ESP_LOGI(TAG, "Freeing the row_dsc object");
    WITH_UI_LOCK() {
        if(app_root_cont)
            lv_obj_delete(app_root_cont);
        if(row_dsc)
            lv_free(row_dsc);
        if(app_scr)
            lv_obj_delete(app_scr);
    }
    row_dsc = NULL;
    app_root_cont = NULL;
    app_scr = NULL;
}

