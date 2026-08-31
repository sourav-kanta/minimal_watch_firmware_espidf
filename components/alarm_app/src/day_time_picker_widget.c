#include <day_time_picker_widget.h>
#include <ui_utils.h>
#include <global_locks.h>
#include <ui_theme.h>

static const int INDICATOR_DIM_PX = 1;
static const int MAX_PATTERN_LEN = 20;

static void on_key_press_cb(lv_event_t* event) {
    if(lv_event_get_code(event) != LV_EVENT_KEY) return;
    if(lv_event_get_key(event) != LV_KEY_LEFT && lv_event_get_key(event) != LV_KEY_RIGHT)
        return;
    int val = 0;
    WITH_UI_LOCK() {
        char* str = lv_label_get_text(lv_event_get_target_obj(event));
        assert(str);
        val = str[0] - '0';
        assert(val>=0 && val<=9);
    }
    if(lv_event_get_key(event) == LV_KEY_LEFT) {
        WITH_UI_LOCK() {
            val = ((val - 1) % 10 + 10) % 10;
            lv_label_set_text_fmt(lv_event_get_target_obj(event), "%d", val);
            lv_event_stop_bubbling(event);
        }
    }
    if(lv_event_get_key(event) == LV_KEY_RIGHT) {
        WITH_UI_LOCK() {
            val = ((val + 1) % 10 + 10) % 10;
            lv_label_set_text_fmt(lv_event_get_target_obj(event), "%d", val);
            lv_event_stop_bubbling(event);
        }
    }
}

static lv_obj_t*  draw_focusable_label(lv_obj_t* parent, const lv_font_t* font, const char* lbl_txt) {
    assert(parent);
    lv_obj_t* label_bg = lv_obj_create(parent);
    remove_shadow_and_outline(label_bg);
    make_obj_navigable(label_bg);
    lv_obj_set_style_bg_opa(label_bg, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(label_bg, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(label_bg, LV_BORDER_SIDE_TOP, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(label_bg, INDICATOR_DIM_PX, LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(label_bg, COLOR_THEME_TEXT_HIGLIGHT, LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(label_bg, LV_OPA_COVER, LV_STATE_FOCUSED);

    lv_obj_t* label = lv_label_create(label_bg);
    lv_label_set_text(label, lbl_txt);
    lv_obj_set_style_text_font(label, font, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label, COLOR_THEME_TEXT_HIGLIGHT, LV_STATE_FOCUSED);
    lv_obj_set_style_text_color(label, COLOR_THEME_TEXT_PRIMARY, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, LV_STATE_DEFAULT);
    make_obj_navigable(label);
    lv_obj_add_event_cb(label, on_key_press_cb, LV_EVENT_KEY, NULL);

    lv_obj_set_size(label_bg, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    return label;
}

lv_obj_t* day_time_picker_widget_draw(picker_params_t* params,  lv_obj_t** out_labels) {
    assert(params);
    assert(params->font);
    assert(params->parent);
    assert(params->pattern);
    assert(out_labels);
    lv_obj_t* sel_base = NULL;
    WITH_UI_LOCK() {
        sel_base = lv_obj_create(params->parent);
        remove_shadow_and_outline(sel_base);
        lv_obj_set_layout(sel_base, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(sel_base, LV_FLEX_FLOW_ROW);
        make_obj_navigable(sel_base);
        lv_obj_set_style_bg_opa(sel_base, LV_OPA_TRANSP, LV_STATE_DEFAULT);
        lv_obj_set_style_border_side(sel_base, LV_BORDER_SIDE_BOTTOM, LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(sel_base, 2, LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(sel_base, COLOR_THEME_TEXT_HIGLIGHT, LV_STATE_DEFAULT);
        lv_obj_set_style_border_opa(sel_base, LV_OPA_TRANSP, LV_STATE_DEFAULT);
        lv_obj_set_style_border_opa(sel_base, LV_OPA_COVER, LV_STATE_FOCUSED);
    
        size_t pattern_len = strnlen(params->pattern, MAX_PATTERN_LEN);
        int lbl_count = 0;
        for(int i=0; i< pattern_len; i++) {
            if(params->pattern[i] == '*') {
                lv_obj_t* label = draw_focusable_label(sel_base, params->font, "0");
                out_labels[lbl_count++] = label;
            }
            else if(params->pattern[i] >= '0' && params->pattern[i] <= '9') {
                char label_txt[2] = { params->pattern[i], '\0' };
                lv_obj_t* label = draw_focusable_label(sel_base, params->font, label_txt);
                out_labels[lbl_count++] = label;
            }
            else {
                lv_obj_t* sep = lv_label_create(sel_base);
                lv_label_set_text_fmt(sep, "%c", params->pattern[i]);
                lv_obj_set_style_text_color(sep, COLOR_THEME_TEXT_PRIMARY, LV_STATE_DEFAULT);
                lv_obj_set_style_text_font(sep, params->font, LV_STATE_DEFAULT);
                lv_obj_set_style_margin_top(sep, INDICATOR_DIM_PX, LV_STATE_DEFAULT);
            }
        }
        lv_obj_set_size(sel_base, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    }
    return sel_base;    
}

void day_time_picker_widget_get_selected_value(const char* pattern, lv_obj_t** labels, char* out_val) {
    assert(pattern);
    assert(labels);
    assert(out_val);
    size_t pattern_len = strnlen(pattern, MAX_PATTERN_LEN);
    int lbl_count = 0;
    for(int i=0; i<pattern_len; i++) {
        if(pattern[i] == '*') {
            lv_obj_t* label = labels[lbl_count++];
            assert(label);
            char* txt = lv_label_get_text(label);
            assert(txt);
            out_val[i] = txt[0];
            assert(out_val[i]>='0' && out_val[i]<='9');
        }
        else {
            out_val[i] = pattern[i];
        }
    }
}
