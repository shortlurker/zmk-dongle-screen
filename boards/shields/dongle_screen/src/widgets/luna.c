/*
 * Copyright (c) 2025 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/wpm_state_changed.h>
#include <zmk/wpm.h>

#include <zmk/hid_indicators.h>
#include <zmk/events/hid_indicators_changed.h>
#include "luna.h"

#define LED_CLCK 0x02

#define SRC(array) (const void **)array, sizeof(array) / sizeof(lv_img_dsc_t *)

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

LV_IMG_DECLARE(dog_sit1);
LV_IMG_DECLARE(dog_sit2);
LV_IMG_DECLARE(dog_walk1);
LV_IMG_DECLARE(dog_walk2);
LV_IMG_DECLARE(dog_run1);
LV_IMG_DECLARE(dog_run2);
LV_IMG_DECLARE(dog_sneak1);
LV_IMG_DECLARE(dog_sneak2);
LV_IMG_DECLARE(dog_bark1);
LV_IMG_DECLARE(dog_bark2);

#define ANIMATION_SPEED_SIT 960
const lv_img_dsc_t *sit_imgs[] = {
    &dog_sit1,
    &dog_sit2,
};

#define ANIMATION_SPEED_WALK 200
const lv_img_dsc_t *walk_imgs[] = {
    &dog_walk1,
    &dog_walk2,
};

#define ANIMATION_SPEED_RUN 200
const lv_img_dsc_t *run_imgs[] = {
    &dog_run1,
    &dog_run2,
};

#define ANIMATION_SPEED_SNEAK 200
const lv_img_dsc_t *sneak_imgs[] = {
    &dog_sneak1,
    &dog_sneak2,
};

#define ANIMATION_SPEED_BARK 200
const lv_img_dsc_t *bark_imgs[] = {
    &dog_bark1,
    &dog_bark2,
};
struct luna_wpm_status_state {
    uint8_t wpm;
};

enum anim_state {
    anim_state_none,
    anim_state_sit,
    anim_state_walk,
    anim_state_run,
    anim_state_sneak,
    anim_state_bark
} current_anim_state;

static void set_animation(lv_obj_t *animing, struct luna_wpm_status_state state) {
    uint8_t mods = zmk_hid_get_keyboard_report()->body.modifiers;
    bool caps = (zmk_hid_indicators_get_current_profile() & LED_CLCK);

    if (caps) {
        if (current_anim_state != anim_state_bark) {
            lv_animimg_set_src(animing, SRC(bark_imgs));
            lv_animimg_set_duration(animing, ANIMATION_SPEED_BARK);
            lv_animimg_set_repeat_count(animing, LV_ANIM_REPEAT_INFINITE);
            lv_animimg_start(animing);
            current_anim_state = anim_state_bark;
        }
    } else if (mods & (MOD_LCTL | MOD_RCTL)) {
        if (current_anim_state != anim_state_sneak) {
            lv_animimg_set_src(animing, SRC(sneak_imgs));
            lv_animimg_set_duration(animing, ANIMATION_SPEED_SNEAK);
            lv_animimg_set_repeat_count(animing, LV_ANIM_REPEAT_INFINITE);
            lv_animimg_start(animing);
            current_anim_state = anim_state_sneak;
        }
    } else if (state.wpm < 10) {
        if (current_anim_state != anim_state_sit) {
            lv_animimg_set_src(animing, SRC(sit_imgs));
            lv_animimg_set_duration(animing, ANIMATION_SPEED_SIT);
            lv_animimg_set_repeat_count(animing, LV_ANIM_REPEAT_INFINITE);
            lv_animimg_start(animing);
            current_anim_state = anim_state_sit;
        }
    } else if (state.wpm < 70) {
        if (current_anim_state != anim_state_walk) {
            lv_animimg_set_src(animing, SRC(walk_imgs));
            lv_animimg_set_duration(animing, ANIMATION_SPEED_WALK);
            lv_animimg_set_repeat_count(animing, LV_ANIM_REPEAT_INFINITE);
            lv_animimg_start(animing);
            current_anim_state = anim_state_walk;
        }
    } else {
        if (current_anim_state != anim_state_run) {
            lv_animimg_set_src(animing, SRC(run_imgs));
            lv_animimg_set_duration(animing, ANIMATION_SPEED_RUN);
            lv_animimg_set_repeat_count(animing, LV_ANIM_REPEAT_INFINITE);
            lv_animimg_start(animing);
            current_anim_state = anim_state_run;
        }
    }
}

struct luna_wpm_status_state luna_wpm_status_get_state(const zmk_event_t *eh) {
    struct zmk_wpm_state_changed *ev = as_zmk_wpm_state_changed(eh);
    return (struct luna_wpm_status_state){.wpm = ev->state};
};

void luna_wpm_status_update_cb(struct luna_wpm_status_state state) {
    struct zmk_widget_luna *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_animation(widget->obj, state); }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_luna, struct luna_wpm_status_state, luna_wpm_status_update_cb,
                            luna_wpm_status_get_state)

ZMK_SUBSCRIPTION(widget_luna, zmk_wpm_state_changed);
ZMK_SUBSCRIPTION(widget_luna, zmk_hid_indicators_changed);

int zmk_widget_luna_init(struct zmk_widget_luna *widget, lv_obj_t *parent) {
    widget->obj = lv_animimg_create(parent);
    lv_obj_center(widget->obj);

    sys_slist_append(&widgets, &widget->node);

    widget_luna_init();

    return 0;
}

lv_obj_t *zmk_widget_luna_obj(struct zmk_widget_luna *widget) { return widget->obj; }