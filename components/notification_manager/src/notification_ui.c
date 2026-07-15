#include <notification_manager.h>
#include <lvgl.h>
#include <global_locks.h>
#include <esp_log.h>
#include <ui_theme.h>
#include <ui_utils.h>

#define VISIBLE_CARDS       3
#define CARD_HEIGHT         56
#define CARD_PADDING        4
#define CARD_TOTAL_STEP     (CARD_HEIGHT + CARD_PADDING)

static const char* TAG = "Notification UI";
static const lv_coord_t SCREEN_WIDTH             = 128;
static const lv_coord_t SCREEN_HEIGHT            = 160;
static const lv_coord_t NAV_BUTTON_SIZE          = 18;
static const lv_coord_t TEXT_BUTTON_HEIGHT       = 22;
static const lv_coord_t TOP_HEADER_BAR_HEIGHT    = 24;
static const lv_coord_t BOTTOM_FOOTER_BAR_HEIGHT = 30;
static const lv_coord_t APP_ICON_SIZE            = 16;
static const lv_coord_t HEADER_ICON_SIZE         = 12;
static const lv_coord_t CARD_TEXT_WRAPPER_WIDTH  = 68;
static const lv_coord_t CARD_RADIUS              = 6;
static const lv_coord_t BUTTON_RADIUS            = 4;
static const lv_coord_t CARD_BODY_LABEL_HEIGHT   = 24;
static const lv_coord_t WRAPPER_PADDING_ROW      = 2;
static const lv_coord_t EMPTY_WRAPPER_PAD_TOP    = 35;
static const lv_coord_t EMPTY_BADGE_SIZE         = 32;

static lv_obj_t *notification_root_cont = NULL;
static lv_obj_t *notification_scr = NULL;
static lv_obj_t *scroll_container = NULL;
static unsigned int noti_snapshot_len = 0;
static unsigned int noti_curr_base_idx = 0;
static const notification_t* notif_list;
static void card_action_cb(lv_event_t *e);

static void notification_ui_invalidate(void);

struct card_data_t {
    const notification_t* n;
    unsigned int idx;
};

static struct card_data_t card_data[VISIBLE_CARDS]; 

static const void *get_app_icon(phone_app_t app) {
    switch (app) {
        case WHATSAPP:   return LV_SYMBOL_AUDIO;
        case MESSAGE:    return LV_SYMBOL_ENVELOPE;
        case NAVIGATION: return LV_SYMBOL_GPS;
        case CALL:       return LV_SYMBOL_CALL;
        default:         return LV_SYMBOL_BELL;
    }
}

static void auto_scroll_to_card(lv_obj_t* card) {
    if (!card || !scroll_container) return;
    static const lv_coord_t precalculated_scroll_targets[VISIBLE_CARDS] = {0, 0, 16};
    
    if (lv_obj_get_height(card) == 0) {
        WITH_UI_LOCK() {
            struct card_data_t *cd = (struct card_data_t *)lv_obj_get_user_data(card);
        
            if (cd && cd->idx < VISIBLE_CARDS) {
                lv_coord_t backup_scroll_y = precalculated_scroll_targets[cd->idx];
                ESP_LOGD(TAG, "Layout dirty! Using precalculated backup scroll Y: %d for index %d", 
                        backup_scroll_y, cd->idx);
                WITH_UI_LOCK() {
                    lv_obj_scroll_to_y(scroll_container, backup_scroll_y, true);
                }
            } else {
                ESP_LOGE(TAG, "Layout dirty and card metadata missing or out of bounds");
            }
        }
        return;
    }
    lv_area_t card_area;
    lv_obj_get_coords(card, &card_area);
    
    lv_coord_t card_top = card_area.y1;
    lv_coord_t card_bottom = card_area.y2;
    ESP_LOGD(TAG, "Layout stable. Auto scroll check - Top: %d, Bottom: %d", card_top, card_bottom);
    if (card_bottom > SCREEN_HEIGHT) {
        lv_coord_t scroll_y_offset = card_bottom - SCREEN_HEIGHT;
        WITH_UI_LOCK() {
            lv_obj_scroll_to_y(scroll_container, 
                   lv_obj_get_scroll_y(scroll_container) + scroll_y_offset, true);
        }
    }
    else if (card_top < 0) {
        WITH_UI_LOCK() {
            lv_obj_scroll_to_y(scroll_container, lv_obj_get_scroll_y(scroll_container) + card_top, true);
        }
    }
}

static void focus_changed_cb(lv_event_t *e) {
    lv_obj_t *card = lv_event_get_current_target(e);
    lv_obj_t *originator = lv_event_get_target(e);
    if (card != originator) {
        return; 
    }
    auto_scroll_to_card(card);
}

static lv_obj_t* create_nav_button(lv_obj_t *parent, const char *symbol) {
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, NAV_BUTTON_SIZE, NAV_BUTTON_SIZE);
    remove_shadow_and_outline(btn);
    
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, symbol);
    lv_obj_center(lbl);
    lv_obj_set_style_bg_color(btn, COLOR_THEME_TEXT_PRIMARY, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn, COLOR_THEME_TEXT_HIGLIGHT, LV_PART_MAIN | LV_STATE_FOCUSED);
    return btn;
}

static lv_obj_t* create_text_action_button(lv_obj_t *parent, const char *text) {
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_height(btn, TEXT_BUTTON_HEIGHT);
    remove_shadow_and_outline(btn);
    lv_obj_set_style_radius(btn, BUTTON_RADIUS, 0);
    
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_center(lbl);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl, COLOR_THEME_TEXT_PRIMARY, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl, COLOR_THEME_PRIMARY, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_add_event_cb(btn, card_action_cb, LV_EVENT_CLICKED, NULL);
    return btn;
}

static void detail_screen_key_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        lv_obj_t *detail_scr = lv_event_get_current_target(e);
        lv_obj_t *originator = lv_event_get_target(e);
        if (key == LV_KEY_ESC) {
            if (detail_scr == originator) {
                WITH_UI_LOCK() {
                    lv_obj_t *prev_focused = (lv_obj_t *)lv_event_get_user_data(e);
                    if (prev_focused) {
                        lv_group_focus_obj(prev_focused);
                    }
                    lv_obj_delete(detail_scr);
                    lv_event_stop_bubbling(e);
                }
                return;
            }
        }
    }
}

static void title_key_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        if (key == LV_KEY_ENTER) {
            lv_obj_t *lbl_title = lv_event_get_target(e);
            lv_obj_t* parent = lv_obj_get_user_data(lbl_title);
            if(!parent) return;
            struct card_data_t *cd = (struct card_data_t *)lv_obj_get_user_data(parent);
            if(!cd) return;
            const notification_t* data = cd->n;
            if (!data) return;
            WITH_UI_LOCK() {
                lv_obj_t *detail_scr = lv_obj_create(notification_scr);
                remove_shadow_and_outline(detail_scr);
                lv_obj_set_size(detail_scr, SCREEN_WIDTH, SCREEN_HEIGHT);
                lv_obj_set_style_bg_color(detail_scr, COLOR_THEME_PRIMARY, 0);
                lv_obj_set_style_bg_opa(detail_scr, LV_OPA_COVER, 0);
                lv_obj_set_style_pad_all(detail_scr, 0, 0);
                lv_obj_set_layout(detail_scr, LV_LAYOUT_FLEX);
                lv_obj_set_flex_flow(detail_scr, LV_FLEX_FLOW_COLUMN);
                
                lv_obj_t *curr_focused = lv_group_get_focused(lv_group_get_default());
                lv_obj_add_event_cb(detail_scr, detail_screen_key_cb, LV_EVENT_KEY, (void *)curr_focused);
                make_obj_navigable(detail_scr);
                lv_group_focus_obj(detail_scr);

                lv_obj_t *top_bar = lv_obj_create(detail_scr);
                remove_shadow_and_outline(top_bar);
                lv_obj_set_size(top_bar, SCREEN_WIDTH, TOP_HEADER_BAR_HEIGHT);
                lv_obj_set_style_bg_color(top_bar, COLOR_THEME_SECONDARY, 0);
                lv_obj_set_style_bg_opa(top_bar, LV_OPA_COVER, 0);
                lv_obj_set_layout(top_bar, LV_LAYOUT_FLEX);
                lv_obj_set_flex_flow(top_bar, LV_FLEX_FLOW_ROW);
                lv_obj_set_flex_align(top_bar, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
                lv_obj_set_style_pad_hor(top_bar, 6, 0);
                lv_obj_set_style_pad_ver(top_bar, 0, 0);
                lv_obj_set_style_pad_column(top_bar, 6, 0);
                
                lv_obj_t *hdr_icon = lv_image_create(top_bar);
                lv_obj_set_size(hdr_icon, HEADER_ICON_SIZE, HEADER_ICON_SIZE);
                lv_image_set_src(hdr_icon, get_app_icon(data->app));
                lv_obj_set_style_text_color(hdr_icon, COLOR_THEME_TEXT_PRIMARY, 0);

                lv_obj_t *hdr_lbl = lv_label_create(top_bar);
                lv_obj_set_flex_grow(hdr_lbl, 1);
                lv_obj_set_height(hdr_lbl, LV_SIZE_CONTENT);
                lv_obj_set_style_text_font(hdr_lbl, &lv_font_montserrat_10, 0);
                lv_obj_set_style_text_color(hdr_lbl, COLOR_THEME_TEXT_PRIMARY, 0);
                lv_label_set_text(hdr_lbl, data->app_name);
                lv_label_set_long_mode(hdr_lbl, LV_LABEL_LONG_DOT);

                lv_obj_t *text_area = lv_label_create(detail_scr);
                remove_shadow_and_outline(text_area);
                lv_obj_set_flex_grow(text_area, 1);
                lv_obj_set_width(text_area, SCREEN_WIDTH);
                lv_obj_set_style_bg_opa(text_area, 0, 0);
                lv_obj_set_style_text_font(text_area, &lv_font_montserrat_10, 0);
                lv_obj_set_style_text_color(text_area, COLOR_THEME_TEXT_PRIMARY, 0);
                lv_obj_set_style_pad_hor(text_area, 6, 0);
                lv_obj_set_style_pad_ver(text_area, 6, 0);
                lv_label_set_text(text_area, data->body);

                lv_obj_t *bottom_bar = lv_obj_create(detail_scr);
                remove_shadow_and_outline(bottom_bar);
                lv_obj_set_size(bottom_bar, SCREEN_WIDTH, BOTTOM_FOOTER_BAR_HEIGHT);
                lv_obj_set_style_bg_opa(bottom_bar, 0, 0);
                lv_obj_set_layout(bottom_bar, LV_LAYOUT_FLEX);
                lv_obj_set_flex_flow(bottom_bar, LV_FLEX_FLOW_ROW);
                lv_obj_set_flex_align(bottom_bar, LV_FLEX_ALIGN_CENTER, 
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
                lv_obj_set_style_pad_hor(bottom_bar, 6, 0);
                lv_obj_set_style_pad_ver(bottom_bar, 4, 0);
                lv_obj_set_style_pad_column(bottom_bar, 4, 0);
                lv_obj_add_flag(bottom_bar, LV_OBJ_FLAG_EVENT_BUBBLE);

                if (data->action_handler != NULL) {
                    const char* act_txt = (data->action_name[0] != '\0') ? data->action_name : "OK";
                    lv_obj_t *btn_ok = create_text_action_button(bottom_bar, act_txt);
                    lv_obj_set_style_bg_color(btn_ok, COLOR_THEME_SECONDARY,
                            LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(btn_ok, COLOR_THEME_TERTIARY,
                            LV_PART_MAIN | LV_STATE_FOCUSED);
                    lv_obj_set_flex_grow(btn_ok, 1); 
                    make_obj_navigable(btn_ok);
                }
                const char* dsm_txt = (data->dismiss_text[0] != '\0') ? data->dismiss_text : "CLOSE";
                lv_obj_t *btn_cl = create_text_action_button(bottom_bar, dsm_txt);
                lv_obj_set_style_bg_color(btn_cl, COLOR_THEME_SECONDARY,
                        LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_color(btn_cl, COLOR_THEME_TERTIARY,
                        LV_PART_MAIN | LV_STATE_FOCUSED);
                lv_obj_set_flex_grow(btn_cl, 1); 
                make_obj_navigable(btn_cl);

                lv_event_stop_bubbling(e);
            }
        }
    }
}

static void dismiss_handler_cb(lv_event_t* e) {
    struct card_data_t* data = (struct card_data_t*)lv_event_get_user_data(e);
    unsigned int abs_idx = data->idx + noti_curr_base_idx;
    
    if(noti_curr_base_idx + VISIBLE_CARDS >= noti_snapshot_len) {
        if(noti_curr_base_idx != 0)
            noti_curr_base_idx--;
    }

    if(noti_snapshot_len <= VISIBLE_CARDS) 
        noti_curr_base_idx = 0;
    data->n->dismiss_handler(abs_idx);
    
    notification_ui_invalidate();
    lv_event_stop_processing(e);
}

static void populate_cards(lv_obj_t* parent, const notification_t* notif_list,
                             unsigned int len, int focus) {
    
    unsigned int display_card_len = len < noti_curr_base_idx + VISIBLE_CARDS ?
                                    len : VISIBLE_CARDS + noti_curr_base_idx;
    lv_obj_t* to_focus = NULL;
    bool no_cards = true;
    for (unsigned int i = noti_curr_base_idx; i < display_card_len; i++) {
        
        no_cards = false;
        const notification_t *data = &notif_list[i];
        WITH_UI_LOCK() {
            lv_obj_t *card = lv_obj_create(parent);
            lv_obj_set_size(card, lv_pct(100), CARD_HEIGHT);
            remove_shadow_and_outline(card);
            
            lv_obj_set_layout(card, LV_LAYOUT_FLEX);
            lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_hor(card, 4, 0);
            lv_obj_set_style_pad_ver(card, 0, 0);
            lv_obj_set_style_pad_column(card, 4, 0);
            lv_obj_set_style_bg_color(card, COLOR_THEME_SECONDARY, 
                    LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(card, COLOR_THEME_PRIMARY,
                    LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(card, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(card, CARD_RADIUS, 0);
            lv_obj_set_style_bg_color(card, COLOR_THEME_FOCUS_BG,
                    LV_PART_MAIN | LV_STATE_FOCUSED);
            lv_obj_set_style_border_color(card, COLOR_THEME_ACCENT,
                    LV_PART_MAIN | LV_STATE_FOCUSED);
            
            make_obj_navigable(card);
            if(i - noti_curr_base_idx == focus) {
                to_focus = card;
            }
            
            lv_obj_add_event_cb(card, focus_changed_cb, LV_EVENT_FOCUSED, NULL);
            lv_obj_add_event_cb(card, card_action_cb, LV_EVENT_KEY, NULL);

            unsigned int relative_index = i-noti_curr_base_idx;
            card_data[relative_index].n = data;
            card_data[relative_index].idx = relative_index;
            lv_obj_set_user_data(card, (void *)&card_data[relative_index]);

            lv_obj_t *icon = lv_image_create(card);
            lv_obj_set_size(icon, APP_ICON_SIZE, APP_ICON_SIZE);
            lv_obj_set_style_text_color(icon, COLOR_THEME_TEXT_PRIMARY, 0); 
            lv_obj_set_style_image_recolor_opa(icon, LV_OPA_COVER, 0);
            lv_image_set_src(icon, get_app_icon(data->app));

            lv_obj_t* text_wrapper = lv_obj_create(card);
            remove_shadow_and_outline(text_wrapper);
            lv_obj_add_flag(text_wrapper, LV_OBJ_FLAG_EVENT_BUBBLE);
            lv_obj_set_layout(text_wrapper, LV_LAYOUT_FLEX);
            lv_obj_set_flex_flow(text_wrapper, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_size(text_wrapper, CARD_TEXT_WRAPPER_WIDTH, LV_SIZE_CONTENT); 
            lv_obj_set_style_pad_all(text_wrapper, 0, 0);
            lv_obj_set_style_pad_row(text_wrapper, 1, 0);
            lv_obj_set_style_bg_opa(text_wrapper, 0, 0);

            lv_obj_t *lbl_title = lv_label_create(text_wrapper);
            lv_obj_set_size(lbl_title, lv_pct(100), LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_10, 0);
            lv_obj_set_style_text_color(lbl_title, COLOR_THEME_TEXT_PRIMARY,
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
            make_obj_navigable(lbl_title); 
            lv_obj_set_style_text_color(lbl_title, COLOR_THEME_TEXT_HIGLIGHT,
                    LV_PART_MAIN | LV_STATE_FOCUSED);
            lv_label_set_text(lbl_title, data->app_name);
            lv_obj_set_user_data(lbl_title, (void*) card); 
            
            lv_obj_add_event_cb(lbl_title, title_key_cb, LV_EVENT_KEY, NULL);

            lv_obj_t *lbl_body = lv_label_create(text_wrapper);
            lv_obj_set_size(lbl_body, lv_pct(100), CARD_BODY_LABEL_HEIGHT);
            lv_label_set_long_mode(lbl_body, LV_LABEL_LONG_DOT);
            lv_obj_set_style_text_font(lbl_body, &lv_font_montserrat_10, 0);
            lv_obj_set_style_text_color(lbl_body, COLOR_THEME_TEXT_SECONDARY, 0);
            lv_label_set_text(lbl_body, data->body);
            
            lv_obj_t* btn_wrapper = lv_obj_create(card);
            remove_shadow_and_outline(btn_wrapper);
            lv_obj_add_flag(btn_wrapper, LV_OBJ_FLAG_EVENT_BUBBLE);
            lv_obj_set_layout(btn_wrapper, LV_LAYOUT_FLEX);
            lv_obj_set_flex_flow(btn_wrapper, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_flex_align(btn_wrapper, LV_FLEX_ALIGN_CENTER, 
                    LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_size(btn_wrapper, NAV_BUTTON_SIZE, LV_SIZE_CONTENT);
            lv_obj_set_style_pad_all(btn_wrapper, 0, 0);
            lv_obj_set_style_pad_row(btn_wrapper, WRAPPER_PADDING_ROW, 0);
            lv_obj_set_style_bg_opa(btn_wrapper, 0, 0);

            lv_obj_t *btn_cl = create_nav_button(btn_wrapper, LV_SYMBOL_CLOSE);
            lv_obj_set_style_bg_color(btn_cl, COLOR_THEME_PRIMARY, 0); 
            lv_obj_set_style_text_color(btn_cl, COLOR_THEME_TEXT_PRIMARY, 0);
            lv_obj_add_event_cb(btn_cl, dismiss_handler_cb, LV_EVENT_CLICKED,
                    (void*)&card_data[relative_index]);
            make_obj_navigable(btn_cl);
        }
        ESP_LOGI(TAG, "Updated cards");
    }
    if(no_cards) {
        WITH_UI_LOCK() {
            lv_obj_t *empty_wrapper = lv_obj_create(parent);
            remove_shadow_and_outline(empty_wrapper);
            lv_obj_set_size(empty_wrapper, lv_pct(100), LV_SIZE_CONTENT);
            lv_obj_set_style_bg_opa(empty_wrapper, 0, 0); 
            
            lv_obj_set_layout(empty_wrapper, LV_LAYOUT_FLEX);
            lv_obj_set_flex_flow(empty_wrapper, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_flex_align(empty_wrapper, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                                     LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_row(empty_wrapper, 10, 0); 
            lv_obj_center(empty_wrapper);
            lv_obj_set_style_pad_top(empty_wrapper, EMPTY_WRAPPER_PAD_TOP, 0);
            lv_obj_t *icon_badge = lv_obj_create(empty_wrapper);
            remove_shadow_and_outline(icon_badge);
            lv_obj_set_size(icon_badge, EMPTY_BADGE_SIZE, EMPTY_BADGE_SIZE);
            lv_obj_set_style_radius(icon_badge, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(icon_badge, COLOR_THEME_SECONDARY, 0);
            lv_obj_set_style_bg_opa(icon_badge, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(icon_badge, COLOR_THEME_FOCUS_BG, 0);
            lv_obj_set_style_border_width(icon_badge, 1, 0);
            
            lv_obj_t *lbl_symbol = lv_label_create(icon_badge);
            lv_label_set_text(lbl_symbol, LV_SYMBOL_OK);
            lv_obj_set_style_text_font(lbl_symbol, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(lbl_symbol, COLOR_THEME_ACCENT, 0); 
            lv_obj_center(lbl_symbol);
            lv_obj_t *text_group = lv_obj_create(empty_wrapper);
            remove_shadow_and_outline(text_group);
            lv_obj_set_size(text_group, lv_pct(100), LV_SIZE_CONTENT);
            lv_obj_set_style_bg_opa(text_group, 0, 0);
            lv_obj_set_layout(text_group, LV_LAYOUT_FLEX);
            lv_obj_set_flex_flow(text_group, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_flex_align(text_group, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                                     LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_row(text_group, WRAPPER_PADDING_ROW, 0); 
            lv_obj_t *lbl_sub = lv_label_create(text_group);
            lv_label_set_text(lbl_sub, "No new notifications.");
            lv_obj_set_style_text_font(lbl_sub, &lv_font_montserrat_10, 0);
            lv_obj_set_style_text_color(lbl_sub, COLOR_THEME_TEXT_PRIMARY, 0); 
            lv_obj_set_style_opa(empty_wrapper, LV_OPA_TRANSP, 0);
            lv_obj_fade_in(empty_wrapper, 250, 50);
        }
    }
    if(to_focus) {
        WITH_UI_LOCK() {
            lv_group_focus_obj(to_focus);
        }
    }
}

static void rebase_and_populate_cards(unsigned int idx) {
    ESP_LOGI(TAG, "Rebasing and repopulating");
    if(idx == 0) {
        if(noti_curr_base_idx == 0) 
            return;
        noti_curr_base_idx--;
    }
    else if(idx == VISIBLE_CARDS-1) {
        if(idx + noti_curr_base_idx + 1 >= noti_snapshot_len) {
           return;
        }
        noti_curr_base_idx++; 
    }
    lv_obj_clean(scroll_container);
    populate_cards(scroll_container, notif_list, noti_snapshot_len, idx);
}

static void card_action_cb(lv_event_t *e) {
    lv_obj_t* originator = lv_event_get_target(e);
    lv_obj_t* curr_focused = lv_event_get_current_target(e);
    if(originator != curr_focused) {
        return;
    }
    if(lv_event_get_code(e) == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        struct card_data_t* card = (struct card_data_t*)lv_obj_get_user_data(curr_focused);
        if(key == LV_KEY_RIGHT) {
            if(card->idx == VISIBLE_CARDS - 1) {
                rebase_and_populate_cards(card->idx);
                lv_event_stop_bubbling(e);
            }
            else {
            }
        }
        if(key == LV_KEY_LEFT) {
            if(card->idx == 0) {
                rebase_and_populate_cards(card->idx);
                lv_event_stop_bubbling(e);
            }
            else {
            }
        }    
    }
}

void notification_manager_draw_notification_page(lv_obj_t* root) {
    if (!root) return;
    ESP_LOGI(TAG, "Inside notifications setup");
    WITH_UI_LOCK() { 
        notification_root_cont = lv_obj_create(root);
        remove_shadow_and_outline(notification_root_cont);
        lv_obj_add_flag(notification_root_cont, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_set_size(notification_root_cont, SCREEN_WIDTH, SCREEN_HEIGHT);
        lv_obj_set_style_bg_color(notification_root_cont, COLOR_THEME_PRIMARY, 0);
        lv_obj_set_style_bg_opa(notification_root_cont, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(notification_root_cont, 0, 0);
        lv_obj_set_style_pad_all(notification_root_cont, 0, 0);
        lv_obj_set_scrollbar_mode(notification_root_cont, LV_SCROLLBAR_MODE_OFF);
        notification_scr = lv_obj_create(notification_root_cont);
        remove_shadow_and_outline(notification_scr);
        lv_obj_set_size(notification_scr, lv_pct(100), lv_pct(100));
        lv_obj_add_flag(notification_scr, LV_OBJ_FLAG_EVENT_BUBBLE);
        
        
        scroll_container = lv_obj_create(notification_scr);
        lv_obj_set_size(scroll_container, SCREEN_WIDTH, SCREEN_HEIGHT);
        remove_shadow_and_outline(scroll_container);
        lv_obj_add_flag(scroll_container, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_set_style_bg_opa(scroll_container, 0, 0); 
        lv_obj_set_style_border_width(scroll_container, 0, 0);
        lv_obj_set_style_pad_all(scroll_container, 2, 0); 
        lv_obj_set_layout(scroll_container, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(scroll_container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(scroll_container, CARD_PADDING, 0);
        lv_obj_set_scrollbar_mode(scroll_container, LV_SCROLLBAR_MODE_OFF);
        lv_obj_remove_flag(scroll_container, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(scroll_container, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_set_style_bg_color(scroll_container, COLOR_THEME_SECONDARY, 0); 
        lv_obj_set_style_bg_opa(scroll_container, LV_OPA_COVER, 0);
        notif_list = notification_manager_retreive_all_notification(&noti_snapshot_len);
        populate_cards(scroll_container, notif_list, noti_snapshot_len, -1);
    }
}

static void notification_ui_invalidate() {
    if(scroll_container == NULL)
        return;
    WITH_UI_LOCK() {
        lv_obj_clean(scroll_container);
    }
    notif_list = notification_manager_retreive_all_notification(&noti_snapshot_len);
    populate_cards(scroll_container, notif_list, noti_snapshot_len, -1);
}

void notification_manager_delete_ui(void) {
    if (notification_root_cont) {
        WITH_UI_LOCK() {
            lv_obj_delete(notification_root_cont);
        }
    }
    notification_root_cont = NULL;
    notification_scr = NULL;
    scroll_container = NULL;
}

