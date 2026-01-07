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
#include "bongo_cat.h"

#define LED_CLCK 0x02

#define SRC(array) (const void **)array, sizeof(array) / sizeof(lv_img_dsc_t *)

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

LV_IMG_DECLARE(cat_idle1);
LV_IMG_DECLARE(cat_idle2);
LV_IMG_DECLARE(cat_idle3);
LV_IMG_DECLARE(cat_idle4);
LV_IMG_DECLARE(cat_right);
LV_IMG_DECLARE(cat_left);
LV_IMG_DECLARE(cat_smash1);
LV_IMG_DECLARE(cat_smash2);
LV_IMG_DECLARE(cat_smash3);
LV_IMG_DECLARE(cat_smash4);

#define ANIMATION_SPEED_IDLE 1000
const lv_img_dsc_t *idle_imgs[] = {
    &cat_idle1,
    &cat_idle2,
    &cat_idle3,
    &cat_idle4,
};

#define ANIMATION_SPEED_SLOW 400
#define ANIMATION_SPEED_MID 300
#define ANIMATION_SPEED_FAST 200
const lv_img_dsc_t *tap_imgs[] = {
    &cat_right,
    &cat_left,
};

#define ANIMATION_SPEED_SMASH 600
const lv_img_dsc_t *smash_imgs[] = {
    &cat_smash2,
    &cat_smash2,
    &cat_smash2,
    &cat_smash2,
    &cat_smash3,
    &cat_smash4,
    &cat_smash4,
    &cat_smash1,
    &cat_smash1,
    &cat_smash1,
};

struct bongo_cat_wpm_status_state {
    uint8_t wpm;
};

enum anim_state {
    anim_state_none,
    anim_state_idle,
    anim_state_slow,
    anim_state_mid,
    anim_state_fast,
    anim_state_smash
} current_anim_state;

static void set_animation(lv_obj_t *animing, struct bongo_cat_wpm_status_state state) {
    bool caps = (zmk_hid_indicators_get_current_profile() & LED_CLCK);

    if (caps) {
        if (current_anim_state != anim_state_smash) {
            lv_animimg_set_src(animing, SRC(smash_imgs));
            lv_animimg_set_duration(animing, ANIMATION_SPEED_SMASH);
            lv_animimg_set_repeat_count(animing, LV_ANIM_REPEAT_INFINITE);
            lv_animimg_start(animing);
            current_anim_state = anim_state_smash;
        }
    } else if (state.wpm < 5) {
        if (current_anim_state != anim_state_idle) {
            lv_animimg_set_src(animing, SRC(idle_imgs));
            lv_animimg_set_duration(animing, ANIMATION_SPEED_IDLE);
            lv_animimg_set_repeat_count(animing, LV_ANIM_REPEAT_INFINITE);
            lv_animimg_start(animing);
            current_anim_state = anim_state_idle;
        }
    } else if (state.wpm < 40) {
        if (current_anim_state != anim_state_slow) {
            lv_animimg_set_src(animing, SRC(tap_imgs));
            lv_animimg_set_duration(animing, ANIMATION_SPEED_SLOW);
            lv_animimg_set_repeat_count(animing, LV_ANIM_REPEAT_INFINITE);
            lv_animimg_start(animing);
            current_anim_state = anim_state_slow;
        }
    } else if (state.wpm < 70) {
        if (current_anim_state != anim_state_mid) {
            lv_animimg_set_src(animing, SRC(tap_imgs));
            lv_animimg_set_duration(animing, ANIMATION_SPEED_MID);
            lv_animimg_set_repeat_count(animing, LV_ANIM_REPEAT_INFINITE);
            lv_animimg_start(animing);
            current_anim_state = anim_state_mid;
        }
    } else {
        if (current_anim_state != anim_state_fast) {
            lv_animimg_set_src(animing, SRC(tap_imgs));
            lv_animimg_set_duration(animing, ANIMATION_SPEED_FAST);
            lv_animimg_set_repeat_count(animing, LV_ANIM_REPEAT_INFINITE);
            lv_animimg_start(animing);
            current_anim_state = anim_state_fast;
        }
    }
}

struct bongo_cat_wpm_status_state bongo_cat_wpm_status_get_state(const zmk_event_t *eh) {
    struct zmk_wpm_state_changed *ev = as_zmk_wpm_state_changed(eh);
    return (struct bongo_cat_wpm_status_state){.wpm = ev->state};
};

void bongo_cat_wpm_status_update_cb(struct bongo_cat_wpm_status_state state) {
    struct zmk_widget_bongo_cat *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_animation(widget->obj, state); }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_bongo_cat, struct bongo_cat_wpm_status_state, bongo_cat_wpm_status_update_cb,
                            bongo_cat_wpm_status_get_state)

ZMK_SUBSCRIPTION(widget_bongo_cat, zmk_wpm_state_changed);
ZMK_SUBSCRIPTION(widget_bongo_cat, zmk_hid_indicators_changed);

int zmk_widget_bongo_cat_init(struct zmk_widget_bongo_cat *widget, lv_obj_t *parent) {
    widget->obj = lv_animimg_create(parent);
    lv_obj_center(widget->obj);

    sys_slist_append(&widgets, &widget->node);

    widget_bongo_cat_init();

    return 0;
}

lv_obj_t *zmk_widget_bongo_cat_obj(struct zmk_widget_bongo_cat *widget) { return widget->obj; }