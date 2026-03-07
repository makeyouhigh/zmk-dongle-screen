/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/display/widgets/layer_status.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/endpoints.h>
#include <zmk/keymap.h>

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct layer_status_state
{
    uint8_t index;
    const char *label;
};

static void set_layer_symbol(lv_obj_t *label, struct layer_status_state state)
{
    /* 1. 버퍼 크기를 32로 확장 (16은 Shift 키 상태값을 덮어버릴 정도로 좁습니다) */
    char text[32] = {};

    if (state.label == NULL)
    {
        snprintf(text, sizeof(text), "%i", state.index);
        lv_label_set_recolor(label, false);
        lv_label_set_text(label, text);
    }
    else
    {
        /* 2. 컬러 태그 로직 보정 및 닫는 샵(#) 추가 (반응속도 해결) */
        if (strcmp(state.label, "Orange") == 0) {
            const char *layer_color = "ffa500";
            snprintf(text, sizeof(text), "#%s %s#", layer_color, state.label);
            lv_label_set_recolor(label, true);
        } else if (strcmp(state.label, "Green") == 0) {
            const char *layer_color = "00ff00";
            snprintf(text, sizeof(text), "#%s %s#", layer_color, state.label);
            lv_label_set_recolor(label, true);
        } else {
            /* 3. 일반 레이어는 색상 기능을 꺼야 메모리 간섭이 안 일어납니다 */
            snprintf(text, sizeof(text), "%s", state.label);
            lv_label_set_recolor(label, false);
        }
        
        lv_label_set_text(label, text);
    }
}

static void layer_status_update_cb(struct layer_status_state state)
{
    struct zmk_widget_layer_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_layer_symbol(widget->obj, state); }
}

static struct layer_status_state layer_status_get_state(const zmk_event_t *eh)
{
    // 이벤트 데이터에서 직접 현재 레이어 번호를 낚아챕니다.
    const struct zmk_layer_state_changed *ev = as_zmk_layer_state_changed(eh);
    
    // 이벤트가 있으면 그 값을, 없으면 시스템 현재값을 가져옵니다.
    uint8_t index = (ev != NULL) ? ev->layer : zmk_keymap_highest_layer_active();

    return (struct layer_status_state){
        .index = index,
        .label = zmk_keymap_layer_name(index)};
}

//static struct layer_status_state layer_status_get_state(const zmk_event_t *eh)
//{
//    uint8_t index = zmk_keymap_highest_layer_active();
//    return (struct layer_status_state){
//        .index = index,
//        .label = zmk_keymap_layer_name(index)};
//}

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer_status, struct layer_status_state, layer_status_update_cb,
                            layer_status_get_state)

ZMK_SUBSCRIPTION(widget_layer_status, zmk_layer_state_changed);

int zmk_widget_layer_status_init(struct zmk_widget_layer_status *widget, lv_obj_t *parent)
{
    widget->obj = lv_label_create(parent);

    lv_obj_set_style_text_font(widget->obj, &lv_font_montserrat_40, 0);

    sys_slist_append(&widgets, &widget->node);

//    widget_layer_status_init();
    _widget_layer_status_init();

    return 0;
}

lv_obj_t *zmk_widget_layer_status_obj(struct zmk_widget_layer_status *widget)
{
    return widget->obj;
}
