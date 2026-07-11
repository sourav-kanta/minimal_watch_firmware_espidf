#include <lvgl.h>
#include <navigation.h>
#include <esp_log.h>

static const char* TAG = "Navigation";

lv_obj_t* find_first_focusable_child_dfs(lv_obj_t* root) {
    if (!root) return NULL;

    if (!lv_group_get_default()) {
        ESP_LOGE(TAG, "Encoder group not setup");
        return NULL;
    }

    uint32_t cnt = lv_obj_get_child_cnt(root);

    // Iterate through children in their exact creation / structural index order
    for (uint32_t i = 0; i < cnt; i++) {
        lv_obj_t* child = lv_obj_get_child(root, i);

        // Base Case: If this immediate child is navigable, we hit a boundary!
        // Return it immediately without checking any of its sub-children.
        if (lv_obj_get_group(child) == lv_group_get_default()) {
            return child;
        }

        // Recursive Case: If it's not navigable (like a layout panel or spacer),
        // we dive deep down this branch to find its first navigable node before moving to sibling 'i+1'.
        lv_obj_t* found = find_first_focusable_child_dfs(child);
        if (found) {
            return found; // Found a navigable item deep in this specific branch!
        }
    }

    return NULL; // This entire branch tree is completely empty of navigable objects
}

lv_obj_t* find_first_focusable_parent_dfs(lv_obj_t* obj) {
    lv_obj_t * parent = lv_obj_get_parent(obj);
    while(parent) {
        if(lv_obj_get_group(parent) == lv_group_get_default()) {
            return parent;
        }
        parent = lv_obj_get_parent(parent);
    }
    return NULL;
}

static lv_obj_t* scan_subtree_focus_sequence(lv_obj_t* root, lv_obj_t* current_focus, 
                                            lv_obj_t** last_found_nav, 
                                            bool* origin_node_passed,
                                            bool is_forward_search) {
    if (!root) return NULL;

    uint32_t child_count = lv_obj_get_child_cnt(root);
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t* child = lv_obj_get_child(root, i);
        bool is_navigable = (lv_obj_get_group(child) == lv_group_get_default());

        if (is_navigable) {
            if (is_forward_search) {
                if (*origin_node_passed) {
                    return child;
                }
                if (child == current_focus) {
                    *origin_node_passed = true;
                }
            } else {
                if (child == current_focus) {
                    return *last_found_nav;
                }
                *last_found_nav = child;
            }
            
            continue; 
        }

        lv_obj_t* resolved_target = scan_subtree_focus_sequence(child, 
                                                                current_focus,
                                                                last_found_nav,
                                                                origin_node_passed,
                                                                is_forward_search);
        if (resolved_target) {
            return resolved_target;
        }
    }
    return NULL;
}

lv_obj_t* find_next_focusable_sibling(lv_obj_t* obj) {
    if (!obj) return NULL;

    lv_obj_t* boundary_roof = find_first_focusable_parent_dfs(obj);
    if (!boundary_roof) return NULL;

    lv_obj_t* last_found_nav = NULL;
    bool origin_node_passed = false;

    return scan_subtree_focus_sequence(boundary_roof, obj, &last_found_nav, &origin_node_passed, true);
}

lv_obj_t* find_prev_focusable_sibling(lv_obj_t* obj) {
    if (!obj) return NULL;

    lv_obj_t* boundary_roof = find_first_focusable_parent_dfs(obj);
    if (!boundary_roof) return NULL;

    lv_obj_t* last_found_nav = NULL;
    bool origin_node_passed = false;

    return scan_subtree_focus_sequence(boundary_roof, obj, &last_found_nav, &origin_node_passed, false);
}

